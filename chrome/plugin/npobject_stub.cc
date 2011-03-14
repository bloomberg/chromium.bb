// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/plugin/npobject_stub.h"

#include "chrome/common/child_process_logging.h"
#include "chrome/plugin/npobject_util.h"
#include "chrome/plugin/plugin_channel_base.h"
#include "chrome/plugin/plugin_thread.h"
#include "content/common/plugin_messages.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"
#include "webkit/plugins/npapi/plugin_constants_win.h"

using WebKit::WebBindings;

NPObjectStub::NPObjectStub(
    NPObject* npobject,
    PluginChannelBase* channel,
    int route_id,
    gfx::NativeViewId containing_window,
    const GURL& page_url)
    : npobject_(npobject),
      channel_(channel),
      route_id_(route_id),
      containing_window_(containing_window),
      page_url_(page_url) {
  channel_->AddRoute(route_id, this, this);

  // We retain the object just as PluginHost does if everything was in-process.
  WebBindings::retainObject(npobject_);
}

NPObjectStub::~NPObjectStub() {
  channel_->RemoveRoute(route_id_);
  if (npobject_)
    WebBindings::releaseObject(npobject_);
}

bool NPObjectStub::Send(IPC::Message* msg) {
  return channel_->Send(msg);
}

void NPObjectStub::OnPluginDestroyed() {
  // We null out the underlying NPObject pointer since it's not valid anymore (
  // ScriptController manually deleted the object).  As a result,
  // OnMessageReceived won't dispatch any more messages.  Since this includes
  // OnRelease, this object won't get deleted until OnChannelError which might
  // not happen for a long time if this renderer process has a long lived
  // plugin instance to the same process.  So we delete this object manually.
  npobject_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* NPObjectStub::GetUnderlyingNPObject() {
  return npobject_;
}

IPC::Channel::Listener* NPObjectStub::GetChannelListener() {
  return static_cast<IPC::Channel::Listener*>(this);
}

bool NPObjectStub::OnMessageReceived(const IPC::Message& msg) {
  child_process_logging::SetActiveURL(page_url_);

  if (!npobject_) {
    if (msg.is_sync()) {
      // The object could be garbage because the frame has gone away, so
      // just send an error reply to the caller.
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }

    return true;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NPObjectStub, msg)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Release, OnRelease);
    IPC_MESSAGE_HANDLER(NPObjectMsg_HasMethod, OnHasMethod);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Invoke, OnInvoke);
    IPC_MESSAGE_HANDLER(NPObjectMsg_HasProperty, OnHasProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_GetProperty, OnGetProperty);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_SetProperty, OnSetProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_RemoveProperty, OnRemoveProperty);
    IPC_MESSAGE_HANDLER(NPObjectMsg_Invalidate, OnInvalidate);
    IPC_MESSAGE_HANDLER(NPObjectMsg_Enumeration, OnEnumeration);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Construct, OnConstruct);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(NPObjectMsg_Evaluate, OnEvaluate);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void NPObjectStub::OnChannelError() {
  // If npobject_ is NULLed out, that means a DeleteSoon is happening.
  if (npobject_)
    delete this;
}

void NPObjectStub::OnRelease(IPC::Message* reply_msg) {
  Send(reply_msg);
  delete this;
}

void NPObjectStub::OnHasMethod(const NPIdentifier_Param& name,
                               bool* result) {
  NPIdentifier id = CreateNPIdentifier(name);
  // If we're in the plugin process, then the stub is holding onto an NPObject
  // from the plugin, so all function calls on it need to go through the
  // functions in NPClass.  If we're in the renderer process, then we just call
  // the NPN_ functions.
  if (IsPluginProcess()) {
    if (npobject_->_class->hasMethod) {
      *result = npobject_->_class->hasMethod(npobject_, id);
    } else {
      *result = false;
    }
  } else {
    *result = WebBindings::hasMethod(0, npobject_, id);
  }
}

void NPObjectStub::OnInvoke(bool is_default,
                            const NPIdentifier_Param& method,
                            const std::vector<NPVariant_Param>& args,
                            IPC::Message* reply_msg) {
  scoped_refptr<PluginChannelBase> local_channel = channel_;
  bool return_value = false;
  NPVariant_Param result_param;
  NPVariant result_var;

  VOID_TO_NPVARIANT(result_var);
  result_param.type = NPVARIANT_PARAM_VOID;

  int arg_count = static_cast<int>(args.size());
  NPVariant* args_var = new NPVariant[arg_count];
  for (int i = 0; i < arg_count; ++i) {
    if (!CreateNPVariant(
            args[i], local_channel, &(args_var[i]), containing_window_,
            page_url_)) {
      NPObjectMsg_Invoke::WriteReplyParams(reply_msg, result_param,
                                           return_value);
      local_channel->Send(reply_msg);
      return;
    }
  }

  if (is_default) {
    if (IsPluginProcess()) {
      if (npobject_->_class->invokeDefault) {
        return_value = npobject_->_class->invokeDefault(
            npobject_, args_var, arg_count, &result_var);
      } else {
        return_value = false;
      }
    } else {
      return_value = WebBindings::invokeDefault(
          0, npobject_, args_var, arg_count, &result_var);
    }
  } else {
    NPIdentifier id = CreateNPIdentifier(method);
    if (IsPluginProcess()) {
      if (npobject_->_class->invoke) {
        return_value = npobject_->_class->invoke(
            npobject_, id, args_var, arg_count, &result_var);
      } else {
        return_value = false;
      }
    } else {
      return_value = WebBindings::invoke(
          0, npobject_, id, args_var, arg_count, &result_var);
    }
  }

  for (int i = 0; i < arg_count; ++i)
    WebBindings::releaseVariantValue(&(args_var[i]));

  delete[] args_var;

  CreateNPVariantParam(
      result_var, local_channel, &result_param, true, containing_window_,
      page_url_);
  NPObjectMsg_Invoke::WriteReplyParams(reply_msg, result_param, return_value);
  local_channel->Send(reply_msg);
}

void NPObjectStub::OnHasProperty(const NPIdentifier_Param& name,
                                 bool* result) {
  NPIdentifier id = CreateNPIdentifier(name);
  if (IsPluginProcess()) {
    if (npobject_->_class->hasProperty) {
      *result = npobject_->_class->hasProperty(npobject_, id);
    } else {
      *result = false;
    }
  } else {
    *result = WebBindings::hasProperty(0, npobject_, id);
  }
}

void NPObjectStub::OnGetProperty(const NPIdentifier_Param& name,
                                 NPVariant_Param* property,
                                 bool* result) {
  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  NPIdentifier id = CreateNPIdentifier(name);

  if (IsPluginProcess()) {
    if (npobject_->_class->getProperty) {
      *result = npobject_->_class->getProperty(npobject_, id, &result_var);
    } else {
      *result = false;
    }
  } else {
    *result = WebBindings::getProperty(0, npobject_, id, &result_var);
  }

  CreateNPVariantParam(
      result_var, channel_, property, true, containing_window_, page_url_);
}

void NPObjectStub::OnSetProperty(const NPIdentifier_Param& name,
                                 const NPVariant_Param& property,
                                 IPC::Message* reply_msg) {
  bool result = false;
  NPVariant result_var;
  VOID_TO_NPVARIANT(result_var);
  NPIdentifier id = CreateNPIdentifier(name);
  NPVariant property_var;
  if (!CreateNPVariant(
          property, channel_, &property_var, containing_window_, page_url_)) {
    NPObjectMsg_SetProperty::WriteReplyParams(reply_msg, result);
    channel_->Send(reply_msg);
    return;
  }

  if (IsPluginProcess()) {
    if (npobject_->_class->setProperty) {
#if defined(OS_WIN)
      static std::wstring filename = StringToLowerASCII(
          PluginThread::current()->plugin_path().BaseName().value());
      static NPIdentifier fullscreen =
          WebBindings::getStringIdentifier("fullScreen");
      if (filename == webkit::npapi::kNewWMPPlugin && id == fullscreen) {
        // Workaround for bug 15985, which is if Flash causes WMP to go
        // full screen a deadlock can occur when WMP calls SetFocus.
        NPObjectMsg_SetProperty::WriteReplyParams(reply_msg, true);
        Send(reply_msg);
        reply_msg = NULL;
      }
#endif
      result = npobject_->_class->setProperty(npobject_, id, &property_var);
    } else {
      result = false;
    }
  } else {
    result = WebBindings::setProperty(0, npobject_, id, &property_var);
  }

  WebBindings::releaseVariantValue(&property_var);

  if (reply_msg) {
    NPObjectMsg_SetProperty::WriteReplyParams(reply_msg, result);
    Send(reply_msg);
  }
}

void NPObjectStub::OnRemoveProperty(const NPIdentifier_Param& name,
                                    bool* result) {
  NPIdentifier id = CreateNPIdentifier(name);
  if (IsPluginProcess()) {
    if (npobject_->_class->removeProperty) {
      *result = npobject_->_class->removeProperty(npobject_, id);
    } else {
      *result = false;
    }
  } else {
    *result = WebBindings::removeProperty(0, npobject_, id);
  }
}

void NPObjectStub::OnInvalidate() {
  if (!IsPluginProcess()) {
    NOTREACHED() << "Should only be called on NPObjects in the plugin";
    return;
  }

  if (!npobject_->_class->invalidate)
    return;

  npobject_->_class->invalidate(npobject_);
}

void NPObjectStub::OnEnumeration(std::vector<NPIdentifier_Param>* value,
                                 bool* result) {
  NPIdentifier* value_np = NULL;
  unsigned int count = 0;
  if (!IsPluginProcess()) {
    *result = WebBindings::enumerate(0, npobject_, &value_np, &count);
  } else {
    if (npobject_->_class->structVersion < NP_CLASS_STRUCT_VERSION_ENUM ||
        !npobject_->_class->enumerate) {
      *result = false;
      return;
    }

    *result = npobject_->_class->enumerate(npobject_, &value_np, &count);
  }

  if (!*result)
    return;

  for (unsigned int i = 0; i < count; ++i) {
    NPIdentifier_Param param;
    CreateNPIdentifierParam(value_np[i], &param);
    value->push_back(param);
  }

  NPN_MemFree(value_np);
}

void NPObjectStub::OnConstruct(const std::vector<NPVariant_Param>& args,
                               IPC::Message* reply_msg) {
  scoped_refptr<PluginChannelBase> local_channel = channel_;
  bool return_value = false;
  NPVariant_Param result_param;
  NPVariant result_var;

  VOID_TO_NPVARIANT(result_var);

  int arg_count = static_cast<int>(args.size());
  NPVariant* args_var = new NPVariant[arg_count];
  for (int i = 0; i < arg_count; ++i) {
    if (!CreateNPVariant(
           args[i], local_channel, &(args_var[i]), containing_window_,
           page_url_)) {
      NPObjectMsg_Invoke::WriteReplyParams(reply_msg, result_param,
                                           return_value);
      local_channel->Send(reply_msg);
      return;
    }
  }

  if (IsPluginProcess()) {
    if (npobject_->_class->structVersion >= NP_CLASS_STRUCT_VERSION_CTOR &&
        npobject_->_class->construct) {
      return_value = npobject_->_class->construct(
          npobject_, args_var, arg_count, &result_var);
    } else {
      return_value = false;
    }
  } else {
    return_value = WebBindings::construct(
        0, npobject_, args_var, arg_count, &result_var);
  }

  for (int i = 0; i < arg_count; ++i)
    WebBindings::releaseVariantValue(&(args_var[i]));

  delete[] args_var;

  CreateNPVariantParam(
      result_var, local_channel, &result_param, true, containing_window_,
      page_url_);
  NPObjectMsg_Invoke::WriteReplyParams(reply_msg, result_param, return_value);
  local_channel->Send(reply_msg);
}

void NPObjectStub::OnEvaluate(const std::string& script,
                              bool popups_allowed,
                              IPC::Message* reply_msg) {
  if (IsPluginProcess()) {
    NOTREACHED() << "Should only be called on NPObjects in the renderer";
    return;
  }

  // Grab a reference to the underlying channel, as the NPObjectStub
  // instance can be destroyed in the context of NPN_Evaluate. This
  // can happen if the containing plugin instance is destroyed in
  // NPN_Evaluate.
  scoped_refptr<PluginChannelBase> local_channel = channel_;

  NPVariant result_var;
  NPString script_string;
  script_string.UTF8Characters = script.c_str();
  script_string.UTF8Length = static_cast<unsigned int>(script.length());

  bool return_value = WebBindings::evaluateHelper(0, popups_allowed, npobject_,
                                                  &script_string, &result_var);

  NPVariant_Param result_param;
  CreateNPVariantParam(
      result_var, local_channel, &result_param, true, containing_window_,
      page_url_);
  NPObjectMsg_Evaluate::WriteReplyParams(reply_msg, result_param, return_value);
  local_channel->Send(reply_msg);
}
