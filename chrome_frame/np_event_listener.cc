// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/np_event_listener.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome_frame/scoped_ns_ptr_win.h"
#include "chrome_frame/ns_associate_iid_win.h"
#include "third_party/xulrunner-sdk/win/include/string/nsEmbedString.h"
#include "third_party/xulrunner-sdk/win/include/dom/nsIDOMElement.h"
#include "third_party/xulrunner-sdk/win/include/dom/nsIDOMEventTarget.h"
#include "third_party/xulrunner-sdk/win/include/dom/nsIDOMEvent.h"

ASSOCIATE_IID(NS_IDOMELEMENT_IID_STR, nsIDOMElement);
ASSOCIATE_IID(NS_IDOMNODE_IID_STR, nsIDOMNode);
ASSOCIATE_IID(NS_IDOMEVENTTARGET_IID_STR, nsIDOMEventTarget);
ASSOCIATE_IID(NS_IDOMEVENTLISTENER_IID_STR, nsIDOMEventListener);

DomEventListener::DomEventListener(NpEventDelegate* delegate)
    : NpEventListenerBase<DomEventListener>(delegate) {
}

DomEventListener::~DomEventListener() {
}

// We implement QueryInterface etc ourselves in order to avoid
// extra dependencies brought on by the NS_IMPL_* macros.
NS_IMETHODIMP DomEventListener::QueryInterface(REFNSIID iid, void** ptr) {
  DCHECK(thread_id_ == ::GetCurrentThreadId());
  nsresult res = NS_NOINTERFACE;

  if (memcmp(&iid, &__uuidof(nsIDOMEventListener), sizeof(nsIID)) == 0 ||
      memcmp(&iid, &__uuidof(nsISupports), sizeof(nsIID)) == 0) {
    *ptr = static_cast<nsIDOMEventListener*>(this);
    AddRef();
    res = NS_OK;
  }

  return res;
}

NS_IMETHODIMP DomEventListener::HandleEvent(nsIDOMEvent *event) {
  DCHECK(thread_id_ == ::GetCurrentThreadId());
  DCHECK(event);

  nsEmbedString tag;
  event->GetType(tag);
  delegate_->OnEvent(WideToUTF8(tag.get()).c_str());

  return NS_OK;
}

bool DomEventListener::Subscribe(NPP instance,
                                 const char* event_names[],
                                 int event_name_count) {
  DCHECK(event_names);
  DCHECK(event_name_count > 0);

  ScopedNsPtr<nsIDOMElement> element;
  bool ret = GetObjectElement(instance, element.Receive());
  if (ret) {
    ScopedNsPtr<nsIDOMEventTarget> target;
    target.QueryFrom(element);
    if (target) {
      for (int i = 0; i < event_name_count && ret; ++i) {
        nsEmbedString name(ASCIIToWide(event_names[i]).c_str());
        // See NPObjectEventListener::Subscribe (below) for a note on why
        // we set the useCapture parameter to PR_FALSE.
        nsresult res = target->AddEventListener(name, this, PR_FALSE);
        DCHECK(res == NS_OK) << "AddEventListener: " << event_names[i];
        ret = NS_SUCCEEDED(res);
      }
    } else {
      DLOG(ERROR) << "failed to get nsIDOMEventTarget";
      ret = false;
    }
  }

  return ret;
}

bool DomEventListener::Unsubscribe(NPP instance,
                                   const char* event_names[],
                                   int event_name_count) {
  DCHECK(event_names);
  DCHECK(event_name_count > 0);

  ScopedNsPtr<nsIDOMElement> element;
  bool ret = GetObjectElement(instance, element.Receive());
  if (ret) {
    ScopedNsPtr<nsIDOMEventTarget> target;
    target.QueryFrom(element);
    if (target) {
      for (int i = 0; i < event_name_count && ret; ++i) {
        nsEmbedString name(ASCIIToWide(event_names[i]).c_str());
        nsresult res = target->RemoveEventListener(name, this, PR_FALSE);
        DCHECK(res == NS_OK) << "RemoveEventListener: " << event_names[i];
        ret = NS_SUCCEEDED(res) && ret;
      }
    } else {
      DLOG(ERROR) << "failed to get nsIDOMEventTarget";
      ret = false;
    }
  }

  return ret;
}

bool DomEventListener::GetObjectElement(NPP instance, nsIDOMElement** element) {
  DCHECK(element);

  ScopedNsPtr<nsIDOMElement> elem;
  // Fetching the dom element works in Firefox, but is not implemented
  // in webkit.
  npapi::GetValue(instance, NPNVDOMElement, elem.Receive());
  if (!elem.get()) {
    DVLOG(1) << "Failed to get NPNVDOMElement";
    return false;
  }

  nsEmbedString tag;
  nsresult res = elem->GetTagName(tag);
  if (NS_SUCCEEDED(res)) {
    if (LowerCaseEqualsASCII(tag.get(), tag.get() + tag.Length(), "embed")) {
      ScopedNsPtr<nsIDOMNode> parent;
      elem->GetParentNode(parent.Receive());
      if (parent) {
        elem.Release();
        res = parent.QueryInterface(elem.Receive());
        DCHECK(NS_SUCCEEDED(res));
      }
    }
  } else {
    NOTREACHED() << " GetTagName";
  }

  *element = elem.Detach();

  return *element != NULL;
}

///////////////////////////////////
// NPObjectEventListener

NPObjectEventListener::NPObjectEventListener(NpEventDelegate* delegate)
    : NpEventListenerBase<NPObjectEventListener>(delegate) {
}

NPObjectEventListener::~NPObjectEventListener() {
  DLOG_IF(ERROR, npo_.get() == NULL);
}

NPObject* NPObjectEventListener::GetObjectElement(NPP instance) {
  NPObject* object = NULL;
  // We can't trust the return value from getvalue.
  // In Opera, the return value can be false even though the correct
  // object is returned.
  npapi::GetValue(instance, NPNVPluginElementNPObject, &object);

  if (object) {
    NPIdentifier* ids = GetCachedStringIds();
    NPVariant var;
    if (npapi::GetProperty(instance, object, ids[TAG_NAME], &var)) {
      DCHECK(NPVARIANT_IS_STRING(var));
      const NPString& np_tag = NPVARIANT_TO_STRING(var);
      std::string tag(np_tag.UTF8Characters, np_tag.UTF8Length);
      npapi::ReleaseVariantValue(&var);

      if (lstrcmpiA(tag.c_str(), "embed") == 0) {
        // We've got the <embed> element but we really want
        // the <object> element.
        if (npapi::GetProperty(instance, object, ids[PARENT_ELEMENT], &var)) {
          npapi::ReleaseObject(object);
          object = NULL;
          if (NPVARIANT_IS_OBJECT(var)) {
            object = NPVARIANT_TO_OBJECT(var);
          } else {
            DVLOG(1) << __FUNCTION__ << " Could not find our parent";
          }
        }
      } else {
        DVLOG(1) << __FUNCTION__ << " got " << tag;
      }
    } else {
      DVLOG(1) << __FUNCTION__ << " failed to get the element's tag";
    }
  } else {
    DLOG(WARNING) << __FUNCTION__ << " failed to get NPNVPluginElementNPObject";
  }

  return object;
}

// Implementation of NpEventListener
bool NPObjectEventListener::Subscribe(NPP instance,
                                      const char* event_names[],
                                      int event_name_count) {
  DCHECK(event_names);
  DCHECK(event_name_count > 0);
  DCHECK(npo_.get() == NULL);

  ScopedNpObject<> plugin_element(GetObjectElement(instance));
  if (!plugin_element.get())
    return false;

  // This object seems to be getting leaked :-(
  bool ret = false;
  npo_.Attach(reinterpret_cast<Npo*>(
      npapi::CreateObject(instance, PluginClass())));

  if (!npo_.get()) {
    NOTREACHED() << "createobject";
  } else {
    npo_->Initialize(this);
    ret = true;

    NPIdentifier* ids = GetCachedStringIds();

    NPVariant args[3];
    OBJECT_TO_NPVARIANT(npo_, args[1]);
    // We don't want to set 'capture' (last parameter) to true.
    // If we do, then in Opera, we'll simply not get callbacks unless
    // the target <object> or <embed> element we're syncing with has its
    // on[event] property assigned to some function handler. weird.
    // Ideally though we'd like to set capture to true since we'd like to
    // only be triggered for this particular object (and not for bubbling
    // events, but alas it's not meant to be.
    BOOLEAN_TO_NPVARIANT(false, args[2]);
    for (int i = 0; i < event_name_count; ++i) {
      ScopedNpVariant result;
      STRINGZ_TO_NPVARIANT(event_names[i], args[0]);
      ret = npapi::Invoke(instance, plugin_element, ids[ADD_EVENT_LISTENER],
                          args, arraysize(args), &result) && ret;
      if (!ret) {
        DLOG(WARNING) << __FUNCTION__ << " invoke failed for "
                      << event_names[i];
        break;
      }
    }
  }

  return ret;
}

bool NPObjectEventListener::Unsubscribe(NPP instance,
                                        const char* event_names[],
                                        int event_name_count) {
  DCHECK(event_names);
  DCHECK(event_name_count > 0);
  DCHECK(npo_.get() != NULL);

  ScopedNpObject<> plugin_element(GetObjectElement(instance));
  if (!plugin_element.get())
    return false;

  NPIdentifier* ids = GetCachedStringIds();

  NPVariant args[3];
  OBJECT_TO_NPVARIANT(npo_, args[1]);
  BOOLEAN_TO_NPVARIANT(false, args[2]);
  for (int i = 0; i < event_name_count; ++i) {
    // TODO(tommi): look into why chrome isn't releasing the reference
    //  count here.  As it stands the reference count doesn't go down
    //  and as a result, the NPO gets leaked.
    ScopedNpVariant result;
    STRINGZ_TO_NPVARIANT(event_names[i], args[0]);
    bool ret = npapi::Invoke(instance, plugin_element,
        ids[REMOVE_EVENT_LISTENER], args, arraysize(args), &result);
    DLOG_IF(ERROR, !ret) << __FUNCTION__ << " invoke: " << ret;
  }

  npo_.Free();

  return true;
}

void NPObjectEventListener::HandleEvent(Npo* npo, NPObject* event) {
  DCHECK(npo);
  DCHECK(event);

  NPIdentifier* ids = GetCachedStringIds();
  ScopedNpVariant result;
  bool ret = npapi::GetProperty(npo->npp(), event, ids[TYPE], &result);
  DCHECK(ret) << "getproperty(type)";
  if (ret) {
    DCHECK(NPVARIANT_IS_STRING(result));
    // Opera doesn't zero terminate its utf8 strings.
    const NPString& type = NPVARIANT_TO_STRING(result);
    std::string zero_terminated(type.UTF8Characters, type.UTF8Length);
    DVLOG(1) << "handleEvent: " << zero_terminated;
    delegate_->OnEvent(zero_terminated.c_str());
  }
}

NPClass* NPObjectEventListener::PluginClass() {
  static NPClass _np_class = {
    NP_CLASS_STRUCT_VERSION,
    reinterpret_cast<NPAllocateFunctionPtr>(AllocateObject),
    reinterpret_cast<NPDeallocateFunctionPtr>(DeallocateObject),
    NULL,  // invalidate
    reinterpret_cast<NPHasMethodFunctionPtr>(HasMethod),
    reinterpret_cast<NPInvokeFunctionPtr>(Invoke),
    NULL,  // InvokeDefault,
    NULL,  // HasProperty,
    NULL,  // GetProperty,
    NULL,  // SetProperty,
    NULL   // construct
  };

  return &_np_class;
}

bool NPObjectEventListener::HasMethod(NPObjectEventListener::Npo* npo,
                                      NPIdentifier name) {
  NPIdentifier* ids = GetCachedStringIds();
  if (name == ids[HANDLE_EVENT])
    return true;

  return false;
}

bool NPObjectEventListener::Invoke(NPObjectEventListener::Npo* npo,
                                   NPIdentifier name, const NPVariant* args,
                                   uint32_t arg_count, NPVariant* result) {
  NPIdentifier* ids = GetCachedStringIds();
  if (name != ids[HANDLE_EVENT])
    return false;

  if (arg_count != 1 || !NPVARIANT_IS_OBJECT(args[0])) {
    NOTREACHED();
  } else {
    NPObject* ev = NPVARIANT_TO_OBJECT(args[0]);
    npo->listener()->HandleEvent(npo, ev);
  }

  return true;
}

NPObject* NPObjectEventListener::AllocateObject(NPP instance,
                                                NPClass* class_name) {
  return new Npo(instance);
}

void NPObjectEventListener::DeallocateObject(NPObjectEventListener::Npo* npo) {
  delete npo;
}

NPIdentifier* NPObjectEventListener::GetCachedStringIds() {
  static NPIdentifier _identifiers[IDENTIFIER_COUNT] = {0};
  if (!_identifiers[0]) {
    const NPUTF8* identifier_names[] = {
      "handleEvent",
      "type",
      "addEventListener",
      "removeEventListener",
      "tagName",
      "parentNode",
    };
    COMPILE_ASSERT(arraysize(identifier_names) == arraysize(_identifiers),
                   mismatched_array_size);
    npapi::GetStringIdentifiers(identifier_names, IDENTIFIER_COUNT,
                                _identifiers);
    for (int i = 0; i < IDENTIFIER_COUNT; ++i) {
      DCHECK(_identifiers[i] != 0);
    }
  }
  return _identifiers;
}
