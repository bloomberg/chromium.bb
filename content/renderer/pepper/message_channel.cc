// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/message_channel.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/pepper/host_array_buffer_var.h"
#include "content/renderer/pepper/npapi_glue.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/v8_var_converter.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebSerializedScriptValue.h"
#include "v8/include/v8.h"

using ppapi::ArrayBufferVar;
using ppapi::PpapiGlobals;
using ppapi::StringVar;
using blink::WebBindings;
using blink::WebElement;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebPluginContainer;
using blink::WebSerializedScriptValue;

namespace content {

namespace {

const char kPostMessage[] = "postMessage";
const char kV8ToVarConversionError[] = "Failed to convert a PostMessage "
    "argument from a JavaScript value to a PP_Var. It may have cycles or be of "
    "an unsupported type.";
const char kVarToV8ConversionError[] = "Failed to convert a PostMessage "
    "argument from a PP_Var to a Javascript value. It may have cycles or be of "
    "an unsupported type.";

// Helper function to get the MessageChannel that is associated with an
// NPObject*.
MessageChannel* ToMessageChannel(NPObject* object) {
  return static_cast<MessageChannel::MessageChannelNPObject*>(object)->
      message_channel.get();
}

NPObject* ToPassThroughObject(NPObject* object) {
  MessageChannel* channel = ToMessageChannel(object);
  return channel ? channel->passthrough_object() : NULL;
}

// Helper function to determine if a given identifier is equal to kPostMessage.
bool IdentifierIsPostMessage(NPIdentifier identifier) {
  return WebBindings::getStringIdentifier(kPostMessage) == identifier;
}

// Copy a PP_Var in to a PP_Var that is appropriate for sending via postMessage.
// This currently just copies the value.  For a string Var, the result is a
// PP_Var with the a copy of |var|'s string contents and a reference count of 1.
PP_Var CopyPPVar(const PP_Var& var) {
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
    case PP_VARTYPE_NULL:
    case PP_VARTYPE_BOOL:
    case PP_VARTYPE_INT32:
    case PP_VARTYPE_DOUBLE:
      return var;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string)
        return PP_MakeUndefined();
      return StringVar::StringToPPVar(string->value());
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      ArrayBufferVar* buffer = ArrayBufferVar::FromPPVar(var);
      if (!buffer)
        return PP_MakeUndefined();
      PP_Var new_buffer_var = PpapiGlobals::Get()->GetVarTracker()->
          MakeArrayBufferPPVar(buffer->ByteLength());
      DCHECK(new_buffer_var.type == PP_VARTYPE_ARRAY_BUFFER);
      if (new_buffer_var.type != PP_VARTYPE_ARRAY_BUFFER)
        return PP_MakeUndefined();
      ArrayBufferVar* new_buffer = ArrayBufferVar::FromPPVar(new_buffer_var);
      DCHECK(new_buffer);
      if (!new_buffer)
        return PP_MakeUndefined();
      memcpy(new_buffer->Map(), buffer->Map(), buffer->ByteLength());
      return new_buffer_var;
    }
    case PP_VARTYPE_OBJECT:
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
    case PP_VARTYPE_RESOURCE:
      // These types are not supported by PostMessage in-process. In some rare
      // cases with the NaCl plugin, they may be sent but they will be dropped
      // anyway (see crbug.com/318837 for details).
      return PP_MakeUndefined();
  }
  NOTREACHED();
  return PP_MakeUndefined();
}

//------------------------------------------------------------------------------
// Implementations of NPClass functions.  These are here to:
// - Implement postMessage behavior.
// - Forward calls to the 'passthrough' object to allow backwards-compatibility
//   with GetInstanceObject() objects.
//------------------------------------------------------------------------------
NPObject* MessageChannelAllocate(NPP npp, NPClass* the_class) {
  return new MessageChannel::MessageChannelNPObject;
}

void MessageChannelDeallocate(NPObject* object) {
  MessageChannel::MessageChannelNPObject* instance =
      static_cast<MessageChannel::MessageChannelNPObject*>(object);
  delete instance;
}

bool MessageChannelHasMethod(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  // We only handle a function called postMessage.
  if (IdentifierIsPostMessage(name))
    return true;

  // Other method names we will pass to the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::hasMethod(NULL, passthrough, name);
  return false;
}

bool MessageChannelInvoke(NPObject* np_obj, NPIdentifier name,
                          const NPVariant* args, uint32 arg_count,
                          NPVariant* result) {
  if (!np_obj)
    return false;

  // We only handle a function called postMessage.
  if (IdentifierIsPostMessage(name) && (arg_count == 1)) {
    MessageChannel* message_channel = ToMessageChannel(np_obj);
    if (message_channel) {
      message_channel->NPVariantToPPVar(&args[0]);
      return true;
    } else {
      return false;
    }
  }
  // Other method calls we will pass to the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough) {
    return WebBindings::invoke(NULL, passthrough, name, args, arg_count,
                               result);
  }
  return false;
}

bool MessageChannelInvokeDefault(NPObject* np_obj,
                                 const NPVariant* args,
                                 uint32 arg_count,
                                 NPVariant* result) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough) {
    return WebBindings::invokeDefault(NULL, passthrough, args, arg_count,
                                      result);
  }
  return false;
}

bool MessageChannelHasProperty(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  MessageChannel* message_channel = ToMessageChannel(np_obj);
  if (message_channel) {
    if (message_channel->GetReadOnlyProperty(name, NULL))
      return true;
  }

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::hasProperty(NULL, passthrough, name);
  return false;
}

bool MessageChannelGetProperty(NPObject* np_obj, NPIdentifier name,
                               NPVariant* result) {
  if (!np_obj)
    return false;

  // Don't allow getting the postMessage function.
  if (IdentifierIsPostMessage(name))
    return false;

  MessageChannel* message_channel = ToMessageChannel(np_obj);
  if (message_channel) {
    if (message_channel->GetReadOnlyProperty(name, result))
      return true;
  }

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::getProperty(NULL, passthrough, name, result);
  return false;
}

bool MessageChannelSetProperty(NPObject* np_obj, NPIdentifier name,
                               const NPVariant* variant) {
  if (!np_obj)
    return false;

  // Don't allow setting the postMessage function.
  if (IdentifierIsPostMessage(name))
    return false;

  // Invoke on the passthrough object, if we have one.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough)
    return WebBindings::setProperty(NULL, passthrough, name, variant);
  return false;
}

bool MessageChannelEnumerate(NPObject *np_obj, NPIdentifier **value,
                             uint32_t *count) {
  if (!np_obj)
    return false;

  // Invoke on the passthrough object, if we have one, to enumerate its
  // properties.
  NPObject* passthrough = ToPassThroughObject(np_obj);
  if (passthrough) {
    bool success = WebBindings::enumerate(NULL, passthrough, value, count);
    if (success) {
      // Add postMessage to the list and return it.
      if (std::numeric_limits<size_t>::max() / sizeof(NPIdentifier) <=
          static_cast<size_t>(*count) + 1)  // Else, "always false" x64 warning.
        return false;
      NPIdentifier* new_array = static_cast<NPIdentifier*>(
          std::malloc(sizeof(NPIdentifier) * (*count + 1)));
      std::memcpy(new_array, *value, sizeof(NPIdentifier)*(*count));
      new_array[*count] = WebBindings::getStringIdentifier(kPostMessage);
      std::free(*value);
      *value = new_array;
      ++(*count);
      return true;
    }
  }

  // Otherwise, build an array that includes only postMessage.
  *value = static_cast<NPIdentifier*>(malloc(sizeof(NPIdentifier)));
  (*value)[0] = WebBindings::getStringIdentifier(kPostMessage);
  *count = 1;
  return true;
}

NPClass message_channel_class = {
  NP_CLASS_STRUCT_VERSION,
  &MessageChannelAllocate,
  &MessageChannelDeallocate,
  NULL,
  &MessageChannelHasMethod,
  &MessageChannelInvoke,
  &MessageChannelInvokeDefault,
  &MessageChannelHasProperty,
  &MessageChannelGetProperty,
  &MessageChannelSetProperty,
  NULL,
  &MessageChannelEnumerate,
};

}  // namespace

// MessageChannel --------------------------------------------------------------
struct MessageChannel::VarConversionResult {
  VarConversionResult(const ppapi::ScopedPPVar& r, bool s)
      : result(r),
        success(s),
        conversion_completed(true) {}
  VarConversionResult()
      : success(false),
        conversion_completed(false) {}
  ppapi::ScopedPPVar result;
  bool success;
  bool conversion_completed;
};

MessageChannel::MessageChannelNPObject::MessageChannelNPObject() {
}

MessageChannel::MessageChannelNPObject::~MessageChannelNPObject() {}

MessageChannel::MessageChannel(PepperPluginInstanceImpl* instance)
    : instance_(instance),
      passthrough_object_(NULL),
      np_object_(NULL),
      early_message_queue_state_(QUEUE_MESSAGES),
      weak_ptr_factory_(this) {
  // Now create an NPObject for receiving calls to postMessage. This sets the
  // reference count to 1.  We release it in the destructor.
  NPObject* obj = WebBindings::createObject(instance_->instanceNPP(),
                                            &message_channel_class);
  DCHECK(obj);
  np_object_ = static_cast<MessageChannel::MessageChannelNPObject*>(obj);
  np_object_->message_channel = weak_ptr_factory_.GetWeakPtr();
}

void MessageChannel::NPVariantToPPVar(const NPVariant* variant) {
  converted_var_queue_.push_back(VarConversionResult());
  std::list<VarConversionResult>::iterator result_iterator =
      --converted_var_queue_.end();
  switch (variant->type) {
    case NPVariantType_Void:
      NPVariantToPPVarComplete(result_iterator,
          ppapi::ScopedPPVar(PP_MakeUndefined()), true);
      return;
    case NPVariantType_Null:
      NPVariantToPPVarComplete(result_iterator,
          ppapi::ScopedPPVar(PP_MakeNull()), true);
      return;
    case NPVariantType_Bool:
      NPVariantToPPVarComplete(result_iterator,
          ppapi::ScopedPPVar(
              PP_MakeBool(PP_FromBool(NPVARIANT_TO_BOOLEAN(*variant)))),
          true);
      return;
    case NPVariantType_Int32:
      NPVariantToPPVarComplete(result_iterator,
          ppapi::ScopedPPVar(
              PP_MakeInt32(NPVARIANT_TO_INT32(*variant))),
          true);
      return;
    case NPVariantType_Double:
      NPVariantToPPVarComplete(result_iterator,
          ppapi::ScopedPPVar(
              PP_MakeDouble(NPVARIANT_TO_DOUBLE(*variant))),
          true);
      return;
    case NPVariantType_String:
      NPVariantToPPVarComplete(result_iterator,
          ppapi::ScopedPPVar(ppapi::ScopedPPVar::PassRef(),
                             StringVar::StringToPPVar(
                                 NPVARIANT_TO_STRING(*variant).UTF8Characters,
                                 NPVARIANT_TO_STRING(*variant).UTF8Length)),
          true);
      return;
    case NPVariantType_Object: {
      // Calling WebBindings::toV8Value creates a wrapper around NPVariant so it
      // shouldn't result in a deep copy.
      v8::Handle<v8::Value> v8_value = WebBindings::toV8Value(variant);
      V8VarConverter(instance_->pp_instance()).FromV8Value(
          v8_value, v8::Isolate::GetCurrent()->GetCurrentContext(),
          base::Bind(&MessageChannel::NPVariantToPPVarComplete,
                     weak_ptr_factory_.GetWeakPtr(), result_iterator));
      return;
    }
  }
  NPVariantToPPVarComplete(result_iterator,
      ppapi::ScopedPPVar(PP_MakeUndefined()), false);
}

void MessageChannel::PostMessageToJavaScript(PP_Var message_data) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());

  // Because V8 is probably not on the stack for Native->JS calls, we need to
  // enter the appropriate context for the plugin.
  WebPluginContainer* container = instance_->container();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;

  v8::Local<v8::Context> context =
      container->element().document().frame()->mainWorldScriptContext();
  // If the page is being destroyed, the context may be empty.
  if (context.IsEmpty())
    return;
  v8::Context::Scope context_scope(context);

  v8::Handle<v8::Value> v8_val;
  if (!V8VarConverter(instance_->pp_instance()).ToV8Value(
          message_data, context, &v8_val)) {
    PpapiGlobals::Get()->LogWithSource(instance_->pp_instance(),
        PP_LOGLEVEL_ERROR, std::string(), kVarToV8ConversionError);
    return;
  }

  WebSerializedScriptValue serialized_val =
      WebSerializedScriptValue::serialize(v8_val);

  if (instance_->module()->IsProxied()) {
    if (early_message_queue_state_ != SEND_DIRECTLY) {
      // We can't just PostTask here; the messages would arrive out of
      // order. Instead, we queue them up until we're ready to post
      // them.
      early_message_queue_.push_back(serialized_val);
    } else {
      // The proxy sent an asynchronous message, so the plugin is already
      // unblocked. Therefore, there's no need to PostTask.
      DCHECK(early_message_queue_.size() == 0);
      PostMessageToJavaScriptImpl(serialized_val);
    }
  } else {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MessageChannel::PostMessageToJavaScriptImpl,
                   weak_ptr_factory_.GetWeakPtr(),
                   serialized_val));
  }
}

void MessageChannel::StopQueueingJavaScriptMessages() {
  // We PostTask here instead of draining the message queue directly
  // since we haven't finished initializing the PepperWebPluginImpl yet, so
  // the plugin isn't available in the DOM.
  early_message_queue_state_ = DRAIN_PENDING;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&MessageChannel::DrainEarlyMessageQueue,
                 weak_ptr_factory_.GetWeakPtr()));
}

void MessageChannel::QueueJavaScriptMessages() {
  if (early_message_queue_state_ == DRAIN_PENDING)
    early_message_queue_state_ = DRAIN_CANCELLED;
  else
    early_message_queue_state_ = QUEUE_MESSAGES;
}

void MessageChannel::NPVariantToPPVarComplete(
    const std::list<VarConversionResult>::iterator& result_iterator,
    const ppapi::ScopedPPVar& result,
    bool success) {
  *result_iterator = VarConversionResult(result, success);
  std::list<VarConversionResult>::iterator it = converted_var_queue_.begin();
  while (it != converted_var_queue_.end() && it->conversion_completed) {
    if (it->success) {
      PostMessageToNative(it->result.get());
    } else {
      PpapiGlobals::Get()->LogWithSource(instance()->pp_instance(),
          PP_LOGLEVEL_ERROR, std::string(), kV8ToVarConversionError);
    }

    converted_var_queue_.erase(it++);
  }
}

void MessageChannel::DrainEarlyMessageQueue() {
  // Take a reference on the PluginInstance. This is because JavaScript code
  // may delete the plugin, which would destroy the PluginInstance and its
  // corresponding MessageChannel.
  scoped_refptr<PepperPluginInstanceImpl> instance_ref(instance_);

  if (early_message_queue_state_ == DRAIN_CANCELLED) {
    early_message_queue_state_ = QUEUE_MESSAGES;
    return;
  }
  DCHECK(early_message_queue_state_ == DRAIN_PENDING);

  while (!early_message_queue_.empty()) {
    PostMessageToJavaScriptImpl(early_message_queue_.front());
    early_message_queue_.pop_front();
  }
  early_message_queue_state_ = SEND_DIRECTLY;
}

void MessageChannel::PostMessageToJavaScriptImpl(
    const WebSerializedScriptValue& message_data) {
  DCHECK(instance_);

  WebPluginContainer* container = instance_->container();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;

  WebDOMEvent event =
      container->element().document().createEvent("MessageEvent");
  WebDOMMessageEvent msg_event = event.to<WebDOMMessageEvent>();
  msg_event.initMessageEvent("message",  // type
                             false,  // canBubble
                             false,  // cancelable
                             message_data,  // data
                             "",  // origin [*]
                             NULL,  // source [*]
                             "");  // lastEventId
  // [*] Note that the |origin| is only specified for cross-document and server-
  //     sent messages, while |source| is only specified for cross-document
  //     messages:
  //      http://www.whatwg.org/specs/web-apps/current-work/multipage/comms.html
  //     This currently behaves like Web Workers. On Firefox, Chrome, and Safari
  //     at least, postMessage on Workers does not provide the origin or source.
  //     TODO(dmichael):  Add origin if we change to a more iframe-like origin
  //                      policy (see crbug.com/81537)

  container->element().dispatchEvent(msg_event);
}

void MessageChannel::PostMessageToNative(PP_Var message_data) {
  if (instance_->module()->IsProxied()) {
    // In the proxied case, the copy will happen via serializiation, and the
    // message is asynchronous. Therefore there's no need to copy the Var, nor
    // to PostTask.
    PostMessageToNativeImpl(message_data);
  } else {
    // Make a copy of the message data for the Task we will run.
    PP_Var var_copy(CopyPPVar(message_data));

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MessageChannel::PostMessageToNativeImpl,
                   weak_ptr_factory_.GetWeakPtr(),
                   var_copy));
  }
}

void MessageChannel::PostMessageToNativeImpl(PP_Var message_data) {
  instance_->HandleMessage(message_data);
}

MessageChannel::~MessageChannel() {
  WebBindings::releaseObject(np_object_);
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);
}

void MessageChannel::SetPassthroughObject(NPObject* passthrough) {
  // Retain the passthrough object; We need to ensure it lives as long as this
  // MessageChannel.
  if (passthrough)
    WebBindings::retainObject(passthrough);

  // If we had a passthrough set already, release it. Note that we retain the
  // incoming passthrough object first, so that we behave correctly if anyone
  // invokes:
  //   SetPassthroughObject(passthrough_object());
  if (passthrough_object_)
    WebBindings::releaseObject(passthrough_object_);

  passthrough_object_ = passthrough;
}

bool MessageChannel::GetReadOnlyProperty(NPIdentifier key,
                                         NPVariant *value) const {
  std::map<NPIdentifier, ppapi::ScopedPPVar>::const_iterator it =
      internal_properties_.find(key);
  if (it != internal_properties_.end()) {
    if (value)
      return PPVarToNPVariant(it->second.get(), value);
    return true;
  }
  return false;
}

void MessageChannel::SetReadOnlyProperty(PP_Var key, PP_Var value) {
  internal_properties_[PPVarToNPIdentifier(key)] = ppapi::ScopedPPVar(value);
}

}  // namespace content
