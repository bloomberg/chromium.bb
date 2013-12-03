// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_bindings.h"

#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/npapi/bindings/npapi.h"
#include "v8/include/v8.h"

using blink::WebBindings;
using blink::WebElement;
using blink::WebDOMEvent;
using blink::WebDOMMessageEvent;
using blink::WebPluginContainer;
using blink::WebString;

namespace content {

namespace {

BrowserPluginBindings* GetBindings(NPObject* object) {
  return static_cast<BrowserPluginBindings::BrowserPluginNPObject*>(object)->
      message_channel.get();
}

std::string StringFromNPVariant(const NPVariant& variant) {
  if (!NPVARIANT_IS_STRING(variant))
    return std::string();
  const NPString& np_string = NPVARIANT_TO_STRING(variant);
  return std::string(np_string.UTF8Characters, np_string.UTF8Length);
}

bool StringToNPVariant(const std::string &in, NPVariant *variant) {
  size_t length = in.size();
  NPUTF8 *chars = static_cast<NPUTF8 *>(malloc(length));
  if (!chars) {
    VOID_TO_NPVARIANT(*variant);
    return false;
  }
  memcpy(chars, in.c_str(), length);
  STRINGN_TO_NPVARIANT(chars, length, *variant);
  return true;
}

// Depending on where the attribute comes from it could be a string, int32,
// or a double. Javascript tends to produce an int32 or a string, but setting
// the value from the developer tools console may also produce a double.
int IntFromNPVariant(const NPVariant& variant) {
  int value = 0;
  switch (variant.type) {
    case NPVariantType_Double:
      value = NPVARIANT_TO_DOUBLE(variant);
      break;
    case NPVariantType_Int32:
      value = NPVARIANT_TO_INT32(variant);
      break;
    case NPVariantType_String:
      base::StringToInt(StringFromNPVariant(variant), &value);
      break;
    default:
      break;
  }
  return value;
}

//------------------------------------------------------------------------------
// Implementations of NPClass functions.  These are here to:
// - Implement src attribute.
//------------------------------------------------------------------------------
NPObject* BrowserPluginBindingsAllocate(NPP npp, NPClass* the_class) {
  return new BrowserPluginBindings::BrowserPluginNPObject;
}

void BrowserPluginBindingsDeallocate(NPObject* object) {
  BrowserPluginBindings::BrowserPluginNPObject* instance =
      static_cast<BrowserPluginBindings::BrowserPluginNPObject*>(object);
  delete instance;
}

bool BrowserPluginBindingsHasMethod(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->HasMethod(name);
}

bool BrowserPluginBindingsInvoke(NPObject* np_obj, NPIdentifier name,
                                 const NPVariant* args, uint32 arg_count,
                                 NPVariant* result) {
  if (!np_obj)
    return false;

  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->InvokeMethod(name, args, arg_count, result);
}

bool BrowserPluginBindingsInvokeDefault(NPObject* np_obj,
                                        const NPVariant* args,
                                        uint32 arg_count,
                                        NPVariant* result) {
  NOTIMPLEMENTED();
  return false;
}

bool BrowserPluginBindingsHasProperty(NPObject* np_obj, NPIdentifier name) {
  if (!np_obj)
    return false;

  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->HasProperty(name);
}

bool BrowserPluginBindingsGetProperty(NPObject* np_obj, NPIdentifier name,
                                      NPVariant* result) {
  if (!np_obj)
    return false;

  if (!result)
    return false;

  // All attributes from here on rely on the bindings, so retrieve it once and
  // return on failure.
  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  return bindings->GetProperty(name, result);
}

bool BrowserPluginBindingsSetProperty(NPObject* np_obj, NPIdentifier name,
                                      const NPVariant* variant) {
  if (!np_obj)
    return false;
  if (!variant)
    return false;

  // All attributes from here on rely on the bindings, so retrieve it once and
  // return on failure.
  BrowserPluginBindings* bindings = GetBindings(np_obj);
  if (!bindings)
    return false;

  if (variant->type == NPVariantType_Null)
    return bindings->RemoveProperty(np_obj, name);

  return bindings->SetProperty(np_obj, name, variant);
}

bool BrowserPluginBindingsEnumerate(NPObject *np_obj, NPIdentifier **value,
                                    uint32_t *count) {
  NOTIMPLEMENTED();
  return true;
}

NPClass browser_plugin_message_class = {
  NP_CLASS_STRUCT_VERSION,
  &BrowserPluginBindingsAllocate,
  &BrowserPluginBindingsDeallocate,
  NULL,
  &BrowserPluginBindingsHasMethod,
  &BrowserPluginBindingsInvoke,
  &BrowserPluginBindingsInvokeDefault,
  &BrowserPluginBindingsHasProperty,
  &BrowserPluginBindingsGetProperty,
  &BrowserPluginBindingsSetProperty,
  NULL,
  &BrowserPluginBindingsEnumerate,
};

}  // namespace

// BrowserPluginMethodBinding --------------------------------------------------

class BrowserPluginMethodBinding {
 public:
  BrowserPluginMethodBinding(const char name[], uint32 arg_count)
      : name_(name),
        arg_count_(arg_count) {
  }

  virtual ~BrowserPluginMethodBinding() {}

  bool MatchesName(NPIdentifier name) const {
    return WebBindings::getStringIdentifier(name_.c_str()) == name;
  }

  uint32 arg_count() const { return arg_count_; }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) = 0;

 private:
  std::string name_;
  uint32 arg_count_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginMethodBinding);
};

class BrowserPluginBindingAttach: public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingAttach()
      : BrowserPluginMethodBinding(
          browser_plugin::kMethodInternalAttach, 1) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    if (!bindings->instance()->render_view())
      return false;

    scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    v8::Handle<v8::Value> obj(blink::WebBindings::toV8Value(&args[0]));
    scoped_ptr<base::Value> value(
        converter->FromV8Value(obj, bindings->instance()->render_view()->
            GetWebView()->mainFrame()->mainWorldScriptContext()));
    if (!value)
      return false;

    if (!value->IsType(Value::TYPE_DICTIONARY))
      return false;

    scoped_ptr<base::DictionaryValue> extra_params(
        static_cast<base::DictionaryValue*>(value.release()));
    bindings->instance()->Attach(extra_params.Pass());
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingAttach);
};

class BrowserPluginBindingAttachWindowTo : public BrowserPluginMethodBinding {
 public:
  BrowserPluginBindingAttachWindowTo()
      : BrowserPluginMethodBinding(
          browser_plugin::kMethodInternalAttachWindowTo, 2) {
  }

  virtual bool Invoke(BrowserPluginBindings* bindings,
                      const NPVariant* args,
                      NPVariant* result) OVERRIDE {
    blink::WebNode node;
    WebBindings::getNode(NPVARIANT_TO_OBJECT(args[0]), &node);
    int window_id = IntFromNPVariant(args[1]);
    BOOLEAN_TO_NPVARIANT(BrowserPlugin::AttachWindowTo(node, window_id),
                         *result);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginBindingAttachWindowTo);
};

// BrowserPluginPropertyBinding ------------------------------------------------

class BrowserPluginPropertyBinding {
 public:
  explicit BrowserPluginPropertyBinding(const char name[]) : name_(name) {}
  virtual ~BrowserPluginPropertyBinding() {}
  const std::string& name() const { return name_; }
  bool MatchesName(NPIdentifier name) const {
    return WebBindings::getStringIdentifier(name_.c_str()) == name;
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) = 0;
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) = 0;
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) = 0;
  // Updates the DOM Attribute value with the current property value.
  void UpdateDOMAttribute(BrowserPluginBindings* bindings,
                          std::string new_value) {
    bindings->instance()->UpdateDOMAttribute(name(), new_value);
  }
 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBinding);
};

class BrowserPluginPropertyBindingAllowTransparency
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingAllowTransparency()
      : BrowserPluginPropertyBinding(
          browser_plugin::kAttributeAllowTransparency) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    bool allow_transparency =
        bindings->instance()->GetAllowTransparencyAttribute();
    BOOLEAN_TO_NPVARIANT(allow_transparency, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string value = StringFromNPVariant(*variant);
    if (!bindings->instance()->HasDOMAttribute(name())) {
      UpdateDOMAttribute(bindings, value);
      bindings->instance()->ParseAllowTransparencyAttribute();
    } else {
      UpdateDOMAttribute(bindings, value);
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseAllowTransparencyAttribute();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingAllowTransparency);
};

class BrowserPluginPropertyBindingAutoSize
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingAutoSize()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeAutoSize) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    bool auto_size = bindings->instance()->GetAutoSizeAttribute();
    BOOLEAN_TO_NPVARIANT(auto_size, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string value = StringFromNPVariant(*variant);
    if (!bindings->instance()->HasDOMAttribute(name())) {
      UpdateDOMAttribute(bindings, value);
      bindings->instance()->ParseAutoSizeAttribute();
    } else {
      UpdateDOMAttribute(bindings, value);
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseAutoSizeAttribute();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingAutoSize);
};

class BrowserPluginPropertyBindingContentWindow
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingContentWindow()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeContentWindow) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    NPObject* obj = bindings->instance()->GetContentWindow();
    if (obj) {
      result->type = NPVariantType_Object;
      result->value.objectValue = WebBindings::retainObject(obj);
    }
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    return false;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {}
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingContentWindow);
};

class BrowserPluginPropertyBindingMaxHeight
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMaxHeight()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMaxHeight) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int max_height = bindings->instance()->GetMaxHeightAttribute();
    INT32_TO_NPVARIANT(max_height, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = IntFromNPVariant(*variant);
    if (bindings->instance()->GetMaxHeightAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseSizeContraintsChanged();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMaxHeight);
};

class BrowserPluginPropertyBindingMaxWidth
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMaxWidth()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMaxWidth) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int max_width = bindings->instance()->GetMaxWidthAttribute();
    INT32_TO_NPVARIANT(max_width, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = IntFromNPVariant(*variant);
    if (bindings->instance()->GetMaxWidthAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseSizeContraintsChanged();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMaxWidth);
};

class BrowserPluginPropertyBindingMinHeight
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMinHeight()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMinHeight) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int min_height = bindings->instance()->GetMinHeightAttribute();
    INT32_TO_NPVARIANT(min_height, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = IntFromNPVariant(*variant);
    if (bindings->instance()->GetMinHeightAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseSizeContraintsChanged();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMinHeight);
};

class BrowserPluginPropertyBindingMinWidth
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingMinWidth()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeMinWidth) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    int min_width = bindings->instance()->GetMinWidthAttribute();
    INT32_TO_NPVARIANT(min_width, *result);
    return true;
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    int new_value = IntFromNPVariant(*variant);
    if (bindings->instance()->GetMinWidthAttribute() != new_value) {
      UpdateDOMAttribute(bindings, base::IntToString(new_value));
      bindings->instance()->ParseSizeContraintsChanged();
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseSizeContraintsChanged();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingMinWidth);
};

class BrowserPluginPropertyBindingName
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingName()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeName) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    std::string name = bindings->instance()->GetNameAttribute();
    return StringToNPVariant(name, result);
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string new_value = StringFromNPVariant(*variant);
    if (bindings->instance()->GetNameAttribute() != new_value) {
      UpdateDOMAttribute(bindings, new_value);
      bindings->instance()->ParseNameAttribute();
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
    bindings->instance()->ParseNameAttribute();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingName);
};

class BrowserPluginPropertyBindingPartition
    : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingPartition()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributePartition) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    std::string partition_id = bindings->instance()->GetPartitionAttribute();
    return StringToNPVariant(partition_id, result);
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string new_value = StringFromNPVariant(*variant);
    std::string old_value = bindings->instance()->GetPartitionAttribute();
    if (old_value != new_value) {
      UpdateDOMAttribute(bindings, new_value);
      std::string error_message;
      if (!bindings->instance()->ParsePartitionAttribute(&error_message)) {
        // Reset to old value on error.
        UpdateDOMAttribute(bindings, old_value);
        // Exceptions must be set as the last operation before returning to
        // script.
        WebBindings::setException(
            np_obj, static_cast<const NPUTF8 *>(error_message.c_str()));
        return false;
      }
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    std::string error_message;
    if (bindings->instance()->CanRemovePartitionAttribute(&error_message)) {
      bindings->instance()->RemoveDOMAttribute(name());
    } else {
      WebBindings::setException(
          np_obj, static_cast<const NPUTF8 *>(error_message.c_str()));
    }
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingPartition);
};

class BrowserPluginPropertyBindingSrc : public BrowserPluginPropertyBinding {
 public:
  BrowserPluginPropertyBindingSrc()
      : BrowserPluginPropertyBinding(browser_plugin::kAttributeSrc) {
  }
  virtual bool GetProperty(BrowserPluginBindings* bindings,
                           NPVariant* result) OVERRIDE {
    std::string src = bindings->instance()->GetSrcAttribute();
    return StringToNPVariant(src, result);
  }
  virtual bool SetProperty(BrowserPluginBindings* bindings,
                           NPObject* np_obj,
                           const NPVariant* variant) OVERRIDE {
    std::string new_value = StringFromNPVariant(*variant);
    // We should not be issuing navigation IPCs if we attempt to set the
    // src property to the empty string. Instead, we want to simply restore
    // the src attribute back to its old value.
    if (new_value.empty()) {
      return true;
    }
    std::string old_value = bindings->instance()->GetSrcAttribute();
    // If the new value was empty then we're effectively resetting the
    // attribute to the old value here. This will be picked up by <webview>'s
    // mutation observer and will restore the src attribute after it has been
    // removed.
    UpdateDOMAttribute(bindings, new_value);
    std::string error_message;
    if (!bindings->instance()->ParseSrcAttribute(&error_message)) {
      // Reset to old value on error.
      UpdateDOMAttribute(bindings, old_value);
      // Exceptions must be set as the last operation before returning to
      // script.
      WebBindings::setException(
          np_obj, static_cast<const NPUTF8 *>(error_message.c_str()));
      return false;
    }
    return true;
  }
  virtual void RemoveProperty(BrowserPluginBindings* bindings,
                              NPObject* np_obj) OVERRIDE {
    bindings->instance()->RemoveDOMAttribute(name());
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPropertyBindingSrc);
};


// BrowserPluginBindings ------------------------------------------------------

BrowserPluginBindings::BrowserPluginNPObject::BrowserPluginNPObject() {
}

BrowserPluginBindings::BrowserPluginNPObject::~BrowserPluginNPObject() {
}

BrowserPluginBindings::BrowserPluginBindings(BrowserPlugin* instance)
    : instance_(instance),
      np_object_(NULL),
      weak_ptr_factory_(this) {
  NPObject* obj =
      WebBindings::createObject(instance->pluginNPP(),
                                &browser_plugin_message_class);
  np_object_ = static_cast<BrowserPluginBindings::BrowserPluginNPObject*>(obj);
  np_object_->message_channel = weak_ptr_factory_.GetWeakPtr();

  method_bindings_.push_back(new BrowserPluginBindingAttach);
  method_bindings_.push_back(new BrowserPluginBindingAttachWindowTo);

  property_bindings_.push_back(
      new BrowserPluginPropertyBindingAllowTransparency);
  property_bindings_.push_back(new BrowserPluginPropertyBindingAutoSize);
  property_bindings_.push_back(new BrowserPluginPropertyBindingContentWindow);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMaxHeight);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMaxWidth);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMinHeight);
  property_bindings_.push_back(new BrowserPluginPropertyBindingMinWidth);
  property_bindings_.push_back(new BrowserPluginPropertyBindingName);
  property_bindings_.push_back(new BrowserPluginPropertyBindingPartition);
  property_bindings_.push_back(new BrowserPluginPropertyBindingSrc);
}

BrowserPluginBindings::~BrowserPluginBindings() {
  WebBindings::releaseObject(np_object_);
}

bool BrowserPluginBindings::HasMethod(NPIdentifier name) const {
  for (BindingList::const_iterator iter = method_bindings_.begin();
       iter != method_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name))
      return true;
  }
  return false;
}

bool BrowserPluginBindings::InvokeMethod(NPIdentifier name,
                                         const NPVariant* args,
                                         uint32 arg_count,
                                         NPVariant* result) {
  for (BindingList::iterator iter = method_bindings_.begin();
       iter != method_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name) && (*iter)->arg_count() == arg_count)
      return (*iter)->Invoke(this, args, result);
  }
  return false;
}

bool BrowserPluginBindings::HasProperty(NPIdentifier name) const {
  for (PropertyBindingList::const_iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name))
      return true;
  }
  return false;
}

bool BrowserPluginBindings::SetProperty(NPObject* np_obj,
                                        NPIdentifier name,
                                        const NPVariant* variant) {
  for (PropertyBindingList::iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name)) {
      if ((*iter)->SetProperty(this, np_obj, variant)) {
        return true;
      }
      break;
    }
  }
  return false;
}

bool BrowserPluginBindings::RemoveProperty(NPObject* np_obj,
                                           NPIdentifier name) {
  for (PropertyBindingList::iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name)) {
      (*iter)->RemoveProperty(this, np_obj);
      return true;
    }
  }
  return false;
}

bool BrowserPluginBindings::GetProperty(NPIdentifier name, NPVariant* result) {
  for (PropertyBindingList::iterator iter = property_bindings_.begin();
       iter != property_bindings_.end();
       ++iter) {
    if ((*iter)->MatchesName(name))
      return (*iter)->GetProperty(this, result);
  }
  return false;
}

}  // namespace content
