// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ibus_controller.h"

#if defined(HAVE_IBUS)
#include <ibus.h>
#endif

#include <algorithm>  // for std::reverse.
#include <cstdio>
#include <cstring>  // for std::strcmp.
#include <set>
#include <sstream>
#include <stack>
#include <utility>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/input_method/ibus_input_methods.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"

namespace chromeos {
namespace input_method {

namespace {

const char kFallbackLayout[] = "us";

InputMethodDescriptors* GetSupportedInputMethodsInternal(
    const InputMethodWhitelist& whitelist) {
  InputMethodDescriptors* input_methods = new InputMethodDescriptors;
  input_methods->reserve(arraysize(kIBusEngines));
  for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
    input_methods->push_back(InputMethodDescriptor(
        whitelist,
        kIBusEngines[i].input_method_id,
        kIBusEngines[i].xkb_layout_id,
        kIBusEngines[i].language_code));
  }
  return input_methods;
}

}  // namespace

class InputMethodWhitelist {
 public:
  InputMethodWhitelist() {
    for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
      supported_input_methods_.insert(kIBusEngines[i].input_method_id);
    }
    for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
      supported_layouts_.insert(kIBusEngines[i].xkb_layout_id);
    }
  }

  // Returns true if |input_method_id| is whitelisted.
  bool InputMethodIdIsWhitelisted(const std::string& input_method_id) const {
    return (supported_input_methods_.count(input_method_id) > 0);
  }

  // Returns true if |xkb_layout| is supported.
  bool XkbLayoutIsSupported(const std::string& xkb_layout) const {
    return (supported_layouts_.count(xkb_layout) > 0);
  }

 private:
  std::set<std::string> supported_input_methods_;
  std::set<std::string> supported_layouts_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodWhitelist);
};

InputMethodDescriptor::InputMethodDescriptor(
    const InputMethodWhitelist& whitelist,
    const std::string& id,
    const std::string& raw_layout,
    const std::string& language_code)
    : id_(id),
      language_code_(language_code) {
  keyboard_layout_ = kFallbackLayout;
  base::SplitString(raw_layout, ',', &virtual_keyboard_layouts_);

  // Find a valid XKB layout name from the comma-separated list, |raw_layout|.
  // Only the first acceptable XKB layout name in the list is used as the
  // |keyboard_layout| value of the InputMethodDescriptor object to be created
  for (size_t i = 0; i < virtual_keyboard_layouts_.size(); ++i) {
    if (whitelist.XkbLayoutIsSupported(virtual_keyboard_layouts_[i])) {
      keyboard_layout_ = virtual_keyboard_layouts_[i];
      DCHECK(keyboard_layout_.find(",") == std::string::npos);
      break;
    }
  }
}

InputMethodDescriptor::InputMethodDescriptor() {
}

InputMethodDescriptor::~InputMethodDescriptor() {
}

// static
InputMethodDescriptor
InputMethodDescriptor::GetFallbackInputMethodDescriptor() {
  InputMethodDescriptor desc;
  desc.id_ = "xkb:us::eng";
  desc.keyboard_layout_ = kFallbackLayout;
  desc.virtual_keyboard_layouts_.push_back(kFallbackLayout);
  desc.language_code_ = "eng";
  return desc;
}

std::string InputMethodDescriptor::ToString() const {
  std::stringstream stream;
  stream << "id=" << id()
         << ", keyboard_layout=" << keyboard_layout()
         << ", virtual_keyboard_layouts=" << virtual_keyboard_layouts_.size()
         << ", language_code=" << language_code();
  return stream.str();
}

ImeProperty::ImeProperty(const std::string& in_key,
                         const std::string& in_label,
                         bool in_is_selection_item,
                         bool in_is_selection_item_checked,
                         int in_selection_item_id)
    : key(in_key),
      label(in_label),
      is_selection_item(in_is_selection_item),
      is_selection_item_checked(in_is_selection_item_checked),
      selection_item_id(in_selection_item_id) {
  DCHECK(!key.empty());
}

ImeProperty::ImeProperty()
    : is_selection_item(false),
      is_selection_item_checked(false),
      selection_item_id(kInvalidSelectionItemId) {
}

ImeProperty::~ImeProperty() {
}

std::string ImeProperty::ToString() const {
  std::stringstream stream;
  stream << "key=" << key
         << ", label=" << label
         << ", is_selection_item=" << is_selection_item
         << ", is_selection_item_checked=" << is_selection_item_checked
         << ", selection_item_id=" << selection_item_id;
  return stream.str();
}

ImeConfigValue::ImeConfigValue()
    : type(kValueTypeString),
      int_value(0),
      bool_value(false) {
}

ImeConfigValue::~ImeConfigValue() {
}

std::string ImeConfigValue::ToString() const {
  std::stringstream stream;
  stream << "type=" << type;
  switch (type) {
    case kValueTypeString:
      stream << ", string_value=" << string_value;
      break;
    case kValueTypeInt:
      stream << ", int_value=" << int_value;
      break;
    case kValueTypeBool:
      stream << ", bool_value=" << (bool_value ? "true" : "false");
      break;
    case kValueTypeStringList:
      stream << ", string_list_value=";
      for (size_t i = 0; i < string_list_value.size(); ++i) {
        if (i) {
          stream << ",";
        }
        stream << string_list_value[i];
      }
      break;
  }
  return stream.str();
}

#if defined(HAVE_IBUS)
const char kPanelObjectKey[] = "panel-object";

namespace {

// Also defined in chrome/browser/chromeos/language_preferences.h (Chrome tree).
const char kGeneralSectionName[] = "general";
const char kPreloadEnginesConfigName[] = "preload_engines";

// The list of input method property keys that we don't handle.
const char* kInputMethodPropertyKeysBlacklist[] = {
  "setup",  // menu for showing setup dialog used in anthy and hangul.
  "chewing_settings_prop",  // menu for showing setup dialog used in chewing.
  "status",  // used in m17n.
};

const char* Or(const char* str1, const char* str2) {
  return str1 ? str1 : str2;
}

// Returns true if |key| is blacklisted.
bool PropertyKeyIsBlacklisted(const char* key) {
  for (size_t i = 0; i < arraysize(kInputMethodPropertyKeysBlacklist); ++i) {
    if (!std::strcmp(key, kInputMethodPropertyKeysBlacklist[i])) {
      return true;
    }
  }
  return false;
}

// Returns IBusInputContext for |input_context_path|. NULL on errors.
IBusInputContext* GetInputContext(
    const std::string& input_context_path, IBusBus* ibus) {
  GDBusConnection* connection = ibus_bus_get_connection(ibus);
  if (!connection) {
    LOG(ERROR) << "IBusConnection is null";
    return NULL;
  }
  // This function does not issue an IBus IPC.
  IBusInputContext* context = ibus_input_context_get_input_context(
      input_context_path.c_str(), connection);
  if (!context) {
    LOG(ERROR) << "IBusInputContext is null: " << input_context_path;
  }
  return context;
}

// Returns true if |prop| has children.
bool PropertyHasChildren(IBusProperty* prop) {
  return prop && prop->sub_props && ibus_prop_list_get(prop->sub_props, 0);
}

// This function is called by and FlattenProperty() and converts IBus
// representation of a property, |ibus_prop|, to our own and push_back the
// result to |out_prop_list|. This function returns true on success, and
// returns false if sanity checks for |ibus_prop| fail.
bool ConvertProperty(IBusProperty* ibus_prop,
                     int selection_item_id,
                     ImePropertyList* out_prop_list) {
  DCHECK(ibus_prop);
  DCHECK(ibus_prop->key);
  DCHECK(out_prop_list);

  // Sanity checks.
  const bool has_sub_props = PropertyHasChildren(ibus_prop);
  if (has_sub_props && (ibus_prop->type != PROP_TYPE_MENU)) {
    LOG(ERROR) << "The property has sub properties, "
               << "but the type of the property is not PROP_TYPE_MENU";
    return false;
  }
  if ((!has_sub_props) && (ibus_prop->type == PROP_TYPE_MENU)) {
    // This is usually not an error. ibus-daemon sometimes sends empty props.
    VLOG(1) << "Property list is empty";
    return false;
  }
  if (ibus_prop->type == PROP_TYPE_SEPARATOR ||
      ibus_prop->type == PROP_TYPE_MENU) {
    // This is not an error, but we don't push an item for these types.
    return true;
  }

  const bool is_selection_item = (ibus_prop->type == PROP_TYPE_RADIO);
  selection_item_id = is_selection_item ?
      selection_item_id : ImeProperty::kInvalidSelectionItemId;

  bool is_selection_item_checked = false;
  if (ibus_prop->state == PROP_STATE_INCONSISTENT) {
    LOG(WARNING) << "The property is in PROP_STATE_INCONSISTENT, "
                 << "which is not supported.";
  } else if ((!is_selection_item) && (ibus_prop->state == PROP_STATE_CHECKED)) {
    LOG(WARNING) << "PROP_STATE_CHECKED is meaningful only if the type is "
                 << "PROP_TYPE_RADIO.";
  } else {
    is_selection_item_checked = (ibus_prop->state == PROP_STATE_CHECKED);
  }

  if (!ibus_prop->key) {
    LOG(ERROR) << "key is NULL";
  }
  if (ibus_prop->tooltip && (!ibus_prop->tooltip->text)) {
    LOG(ERROR) << "tooltip is NOT NULL, but tooltip->text IS NULL: key="
               << Or(ibus_prop->key, "");
  }
  if (ibus_prop->label && (!ibus_prop->label->text)) {
    LOG(ERROR) << "label is NOT NULL, but label->text IS NULL: key="
               << Or(ibus_prop->key, "");
  }

  // This label will be localized on Chrome side.
  // See src/chrome/browser/chromeos/status/language_menu_l10n_util.h.
  std::string label =
      ((ibus_prop->tooltip &&
        ibus_prop->tooltip->text) ? ibus_prop->tooltip->text : "");
  if (label.empty()) {
    // Usually tooltips are more descriptive than labels.
    label = (ibus_prop->label && ibus_prop->label->text)
        ? ibus_prop->label->text : "";
  }
  if (label.empty()) {
    // ibus-pinyin has a property whose label and tooltip are empty. Fall back
    // to the key.
    label = Or(ibus_prop->key, "");
  }

  out_prop_list->push_back(ImeProperty(ibus_prop->key,
                                       label,
                                       is_selection_item,
                                       is_selection_item_checked,
                                       selection_item_id));
  return true;
}

// Converts |ibus_prop| to |out_prop_list|. Please note that |ibus_prop|
// may or may not have children. See the comment for FlattenPropertyList
// for details. Returns true if no error is found.
// TODO(yusukes): Write unittest.
bool FlattenProperty(IBusProperty* ibus_prop, ImePropertyList* out_prop_list) {
  DCHECK(ibus_prop);
  DCHECK(out_prop_list);

  int selection_item_id = -1;
  std::stack<std::pair<IBusProperty*, int> > prop_stack;
  prop_stack.push(std::make_pair(ibus_prop, selection_item_id));

  while (!prop_stack.empty()) {
    IBusProperty* prop = prop_stack.top().first;
    const int current_selection_item_id = prop_stack.top().second;
    prop_stack.pop();

    // Filter out unnecessary properties.
    if (PropertyKeyIsBlacklisted(prop->key)) {
      continue;
    }

    // Convert |prop| to ImeProperty and push it to |out_prop_list|.
    if (!ConvertProperty(prop, current_selection_item_id, out_prop_list)) {
      return false;
    }

    // Process childrens iteratively (if any). Push all sub properties to the
    // stack.
    if (PropertyHasChildren(prop)) {
      ++selection_item_id;
      for (int i = 0;; ++i) {
        IBusProperty* sub_prop = ibus_prop_list_get(prop->sub_props, i);
        if (!sub_prop) {
          break;
        }
        prop_stack.push(std::make_pair(sub_prop, selection_item_id));
      }
      ++selection_item_id;
    }
  }
  std::reverse(out_prop_list->begin(), out_prop_list->end());

  return true;
}

// Converts IBus representation of a property list, |ibus_prop_list| to our
// own. This function also flatten the original list (actually it's a tree).
// Returns true if no error is found. The conversion to our own type is
// necessary since our language switcher in Chrome tree don't (or can't) know
// IBus types. Here is an example:
//
// ======================================================================
// Input:
//
// --- Item-1
//  |- Item-2
//  |- SubMenuRoot --- Item-3-1
//  |               |- Item-3-2
//  |               |- Item-3-3
//  |- Item-4
//
// (Note: Item-3-X is a selection item since they're on a sub menu.)
//
// Output:
//
// Item-1, Item-2, Item-3-1, Item-3-2, Item-3-3, Item-4
// (Note: SubMenuRoot does not appear in the output.)
// ======================================================================
// TODO(yusukes): Write unittest.
bool FlattenPropertyList(
    IBusPropList* ibus_prop_list, ImePropertyList* out_prop_list) {
  DCHECK(ibus_prop_list);
  DCHECK(out_prop_list);

  IBusProperty* fake_root_prop = ibus_property_new("Dummy.Key",
                                                   PROP_TYPE_MENU,
                                                   NULL, /* label */
                                                   "", /* icon */
                                                   NULL, /* tooltip */
                                                   FALSE, /* sensitive */
                                                   FALSE, /* visible */
                                                   PROP_STATE_UNCHECKED,
                                                   ibus_prop_list);
  g_return_val_if_fail(fake_root_prop, false);
  // Increase the ref count so it won't get deleted when |fake_root_prop|
  // is deleted.
  g_object_ref(ibus_prop_list);
  const bool result = FlattenProperty(fake_root_prop, out_prop_list);
  g_object_unref(fake_root_prop);

  return result;
}

// Debug print function.
const char* PropTypeToString(int prop_type) {
  switch (static_cast<IBusPropType>(prop_type)) {
    case PROP_TYPE_NORMAL:
      return "NORMAL";
    case PROP_TYPE_TOGGLE:
      return "TOGGLE";
    case PROP_TYPE_RADIO:
      return "RADIO";
    case PROP_TYPE_MENU:
      return "MENU";
    case PROP_TYPE_SEPARATOR:
      return "SEPARATOR";
  }
  return "UNKNOWN";
}

// Debug print function.
const char* PropStateToString(int prop_state) {
  switch (static_cast<IBusPropState>(prop_state)) {
    case PROP_STATE_UNCHECKED:
      return "UNCHECKED";
    case PROP_STATE_CHECKED:
      return "CHECKED";
    case PROP_STATE_INCONSISTENT:
      return "INCONSISTENT";
  }
  return "UNKNOWN";
}

// Debug print function.
std::string Spacer(int n) {
  return std::string(n, ' ');
}

std::string PrintPropList(IBusPropList *prop_list, int tree_level);
// Debug print function.
std::string PrintProp(IBusProperty *prop, int tree_level) {
  if (!prop) {
    return "";
  }

  std::stringstream stream;
  stream << Spacer(tree_level) << "=========================" << std::endl;
  stream << Spacer(tree_level) << "key: " << Or(prop->key, "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "icon: " << Or(prop->icon, "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "label: "
         << ((prop->label && prop->label->text) ? prop->label->text : "<none>")
         << std::endl;
  stream << Spacer(tree_level) << "tooptip: "
         << ((prop->tooltip && prop->tooltip->text)
             ? prop->tooltip->text : "<none>") << std::endl;
  stream << Spacer(tree_level) << "sensitive: "
         << (prop->sensitive ? "YES" : "NO") << std::endl;
  stream << Spacer(tree_level) << "visible: " << (prop->visible ? "YES" : "NO")
         << std::endl;
  stream << Spacer(tree_level) << "type: " << PropTypeToString(prop->type)
         << std::endl;
  stream << Spacer(tree_level) << "state: " << PropStateToString(prop->state)
         << std::endl;
  stream << Spacer(tree_level) << "sub_props: "
         << (PropertyHasChildren(prop) ? "" : "<none>") << std::endl;
  stream << PrintPropList(prop->sub_props, tree_level + 1);
  stream << Spacer(tree_level) << "=========================" << std::endl;

  return stream.str();
}

// Debug print function.
std::string PrintPropList(IBusPropList *prop_list, int tree_level) {
  if (!prop_list) {
    return "";
  }

  std::stringstream stream;
  for (int i = 0;; ++i) {
    IBusProperty* prop = ibus_prop_list_get(prop_list, i);
    if (!prop) {
      break;
    }
    stream << PrintProp(prop, tree_level);
  }
  return stream.str();
}

}  // namespace

// The real implementation of the IBusController.
class IBusControllerImpl : public IBusController {
 public:
  IBusControllerImpl()
      : ibus_(NULL),
        ibus_config_(NULL) {
  }

  ~IBusControllerImpl() {
    // Disconnect signals so the handler functions will not be called with
    // |this| which is already freed.
    if (ibus_) {
      g_signal_handlers_disconnect_by_func(
          ibus_,
          reinterpret_cast<gpointer>(G_CALLBACK(IBusBusConnectedThunk)),
          this);
      g_signal_handlers_disconnect_by_func(
          ibus_,
          reinterpret_cast<gpointer>(G_CALLBACK(IBusBusDisconnectedThunk)),
          this);
      g_signal_handlers_disconnect_by_func(
          ibus_,
          reinterpret_cast<gpointer>(
              G_CALLBACK(IBusBusGlobalEngineChangedThunk)),
          this);
      g_signal_handlers_disconnect_by_func(
          ibus_,
          reinterpret_cast<gpointer>(G_CALLBACK(IBusBusNameOwnerChangedThunk)),
          this);

      // Disconnect signals for the panel service as well.
      IBusPanelService* ibus_panel_service = IBUS_PANEL_SERVICE(
          g_object_get_data(G_OBJECT(ibus_), kPanelObjectKey));
      if (ibus_panel_service) {
        g_signal_handlers_disconnect_by_func(
            ibus_panel_service,
            reinterpret_cast<gpointer>(G_CALLBACK(FocusInThunk)),
            this);
        g_signal_handlers_disconnect_by_func(
            ibus_panel_service,
            reinterpret_cast<gpointer>(G_CALLBACK(RegisterPropertiesThunk)),
            this);
        g_signal_handlers_disconnect_by_func(
            ibus_panel_service,
            reinterpret_cast<gpointer>(G_CALLBACK(UpdatePropertyThunk)),
            this);
      }
    }
  }

  virtual void Connect() {
    MaybeRestoreConnections();
  }

  // IBusController override.
  virtual bool StopInputMethodProcess() {
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "StopInputMethodProcess: IBus connection is not alive";
      return false;
    }

    // Ask IBus to exit *asynchronously*.
    ibus_bus_exit_async(ibus_,
                        FALSE  /* do not restart */,
                        -1  /* timeout */,
                        NULL  /* cancellable */,
                        NULL  /* callback */,
                        NULL  /* user_data */);

    if (ibus_config_) {
      // Release |ibus_config_| unconditionally to make sure next
      // IBusConnectionsAreAlive() call will return false.
      g_object_unref(ibus_config_);
      ibus_config_ = NULL;
    }
    return true;
  }

  // IBusController override.
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) {
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "SetImePropertyActivated: IBus connection is not alive";
      return;
    }
    if (key.empty()) {
      return;
    }
    if (input_context_path_.empty()) {
      LOG(ERROR) << "Input context is unknown";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    // Activate the property *asynchronously*.
    ibus_input_context_property_activate(
        context, key.c_str(),
        (activated ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED));

    // We don't have to call ibus_proxy_destroy(context) explicitly here,
    // i.e. we can just call g_object_unref(context), since g_object_unref can
    // trigger both dispose, which is overridden by src/ibusproxy.c, and
    // finalize functions. For details, see
    // http://library.gnome.org/devel/gobject/stable/gobject-memory.html
    g_object_unref(context);
  }

  // IBusController override.
  virtual bool ChangeInputMethod(const std::string& name) {
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "ChangeInputMethod: IBus connection is not alive";
      return false;
    }
    if (name.empty()) {
      return false;
    }
    if (!InputMethodIdIsWhitelisted(name) &&
        name.find(kExtensionImePrefix) != 0) {
      return false;
    }

    // Clear all input method properties unconditionally.
    //
    // When switching to another input method and no text area is focused,
    // RegisterProperties signal for the new input method will NOT be sent
    // until a text area is focused. Therefore, we have to clear the old input
    // method properties here to keep the input method switcher status
    // consistent.
    DoRegisterProperties(NULL);

    // Change the global engine *asynchronously*.
    ibus_bus_set_global_engine_async(ibus_,
                                     name.c_str(),
                                     -1,  // use the default ibus timeout
                                     NULL,  // cancellable
                                     NULL,  // callback
                                     NULL);  // user_data
    return true;
  }

  // IBusController override.
  virtual bool SetImeConfig(const std::string& section,
                            const std::string& config_name,
                            const ImeConfigValue& value) {
    // See comments in GetImeConfig() where ibus_config_get_value() is used.
    if (!IBusConnectionsAreAlive()) {
      LOG(ERROR) << "SetImeConfig: IBus connection is not alive";
      return false;
    }

    bool is_preload_engines = false;

    // Sanity check: do not preload unknown/unsupported input methods.
    std::vector<std::string> string_list;
    if ((value.type == ImeConfigValue::kValueTypeStringList) &&
        (section == kGeneralSectionName) &&
        (config_name == kPreloadEnginesConfigName)) {
      FilterInputMethods(value.string_list_value, &string_list);
      is_preload_engines = true;
    } else {
      string_list = value.string_list_value;
    }

    // Convert the type of |value| from our structure to GVariant.
    GVariant* variant = NULL;
    switch (value.type) {
      case ImeConfigValue::kValueTypeString:
        variant = g_variant_new_string(value.string_value.c_str());
        break;
      case ImeConfigValue::kValueTypeInt:
        variant = g_variant_new_int32(value.int_value);
        break;
      case ImeConfigValue::kValueTypeBool:
        variant = g_variant_new_boolean(value.bool_value);
        break;
      case ImeConfigValue::kValueTypeStringList:
        GVariantBuilder variant_builder;
        g_variant_builder_init(&variant_builder, G_VARIANT_TYPE("as"));
        const size_t size = string_list.size();  // don't use string_list_value.
        for (size_t i = 0; i < size; ++i) {
          g_variant_builder_add(&variant_builder, "s", string_list[i].c_str());
        }
        variant = g_variant_builder_end(&variant_builder);
        break;
    }

    if (!variant) {
      LOG(ERROR) << "SetImeConfig: variant is NULL";
      return false;
    }
    DCHECK(g_variant_is_floating(variant));

    // Set an ibus configuration value *asynchronously*.
    ibus_config_set_value_async(ibus_config_,
                                section.c_str(),
                                config_name.c_str(),
                                variant,
                                -1,  // use the default ibus timeout
                                NULL,  // cancellable
                                SetImeConfigCallback,
                                g_object_ref(ibus_config_));

    // Since |variant| is floating, ibus_config_set_value_async consumes
    // (takes ownership of) the variable.

    if (is_preload_engines) {
      VLOG(1) << "SetImeConfig: " << section << "/" << config_name
              << ": " << value.ToString();
    }
    return true;
  }

  // IBusController override.
  virtual void SendHandwritingStroke(const HandwritingStroke& stroke) {
    if (stroke.size() < 2) {
      LOG(WARNING) << "Empty stroke data or a single dot is passed.";
      return;
    }

    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }

    const size_t raw_stroke_size = stroke.size() * 2;
    scoped_array<double> raw_stroke(new double[raw_stroke_size]);
    for (size_t n = 0; n < stroke.size(); ++n) {
      raw_stroke[n * 2] = stroke[n].first;  // x
      raw_stroke[n * 2 + 1] = stroke[n].second;  // y
    }
    ibus_input_context_process_hand_writing_event(
        context, raw_stroke.get(), raw_stroke_size);
    g_object_unref(context);
  }

  // IBusController override.
  virtual void CancelHandwriting(int n_strokes) {
    IBusInputContext* context = GetInputContext(input_context_path_, ibus_);
    if (!context) {
      return;
    }
    ibus_input_context_cancel_hand_writing(context, n_strokes);
    g_object_unref(context);
  }

  virtual bool InputMethodIdIsWhitelisted(const std::string& input_method_id) {
    return whitelist_.InputMethodIdIsWhitelisted(input_method_id);
  }

  virtual bool XkbLayoutIsSupported(const std::string& xkb_layout) {
    return whitelist_.XkbLayoutIsSupported(xkb_layout);
  }

  virtual InputMethodDescriptor CreateInputMethodDescriptor(
      const std::string& id,
      const std::string& raw_layout,
      const std::string& language_code) {
    return InputMethodDescriptor(whitelist_, id, raw_layout, language_code);
  }

  virtual InputMethodDescriptors* GetSupportedInputMethods() {
    return GetSupportedInputMethodsInternal(whitelist_);
  }

  // IBusController override.
  virtual void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  // IBusController override.
  virtual void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  // Functions that end with Thunk are used to deal with glib callbacks.
  //
  // Note that we cannot use CHROMEG_CALLBACK_0() here as we'll define
  // IBusBusConnected() inline. If we are to define the function outside
  // of the class definition, we should use CHROMEG_CALLBACK_0() here.
  //
  // CHROMEG_CALLBACK_0(Impl,
  //                    void, IBusBusConnected, IBusBus*);
  static void IBusBusConnectedThunk(IBusBus* sender, gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->IBusBusConnected(sender);
  }
  static void IBusBusDisconnectedThunk(IBusBus* sender, gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->IBusBusDisconnected(sender);
  }
  static void IBusBusGlobalEngineChangedThunk(IBusBus* sender,
                                              const gchar* engine_name,
                                              gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->IBusBusGlobalEngineChanged(sender, engine_name);
  }
  static void IBusBusNameOwnerChangedThunk(IBusBus* sender,
                                           const gchar* name,
                                           const gchar* old_name,
                                           const gchar* new_name,
                                           gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->IBusBusNameOwnerChanged(sender, name, old_name, new_name);
  }
  static void FocusInThunk(IBusPanelService* sender,
                           const gchar* input_context_path,
                           gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->FocusIn(sender, input_context_path);
  }
  static void RegisterPropertiesThunk(IBusPanelService* sender,
                                      IBusPropList* prop_list,
                                      gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->RegisterProperties(sender, prop_list);
  }
  static void UpdatePropertyThunk(IBusPanelService* sender,
                                  IBusProperty* ibus_prop,
                                  gpointer userdata) {
    return reinterpret_cast<IBusControllerImpl*>(userdata)
        ->UpdateProperty(sender, ibus_prop);
  }

  // Checks if |ibus_| and |ibus_config_| connections are alive.
  bool IBusConnectionsAreAlive() {
    return ibus_ && ibus_bus_is_connected(ibus_) && ibus_config_;
  }

  // Restores connections to ibus-daemon and ibus-memconf if they are not ready.
  // If both |ibus_| and |ibus_config_| become ready, the function sends a
  // notification to Chrome.
  void MaybeRestoreConnections() {
    if (IBusConnectionsAreAlive()) {
      return;
    }
    MaybeCreateIBus();
    MaybeRestoreIBusConfig();
    if (IBusConnectionsAreAlive()) {
      ConnectPanelServiceSignals();
      VLOG(1) << "Notifying Chrome that IBus is ready.";
      FOR_EACH_OBSERVER(Observer, observers_, OnConnectionChange(true));
    }
  }

  // Creates IBusBus object if it's not created yet.
  void MaybeCreateIBus() {
    if (ibus_) {
      return;
    }

    ibus_init();
    // Establish IBus connection between ibus-daemon to retrieve the list of
    // available input method engines, change the current input method engine,
    // and so on.
    ibus_ = ibus_bus_new();

    // Check the IBus connection status.
    if (!ibus_) {
      LOG(ERROR) << "ibus_bus_new() failed";
      return;
    }
    // Register callback functions for IBusBus signals.
    ConnectIBusSignals();

    // Ask libibus to watch the NameOwnerChanged signal *asynchronously*.
    ibus_bus_set_watch_dbus_signal(ibus_, TRUE);
    // Ask libibus to watch the GlobalEngineChanged signal *asynchronously*.
    ibus_bus_set_watch_ibus_signal(ibus_, TRUE);

    if (ibus_bus_is_connected(ibus_)) {
      VLOG(1) << "IBus connection is ready.";
    }
  }

  // Creates IBusConfig object if it's not created yet AND |ibus_| connection
  // is ready.
  void MaybeRestoreIBusConfig() {
    if (!ibus_) {
      return;
    }

    // Destroy the current |ibus_config_| object. No-op if it's NULL.
    MaybeDestroyIBusConfig();

    if (!ibus_config_) {
      GDBusConnection* ibus_connection = ibus_bus_get_connection(ibus_);
      if (!ibus_connection) {
        VLOG(1) << "Couldn't create an ibus config object since "
                << "IBus connection is not ready.";
        return;
      }
      const gboolean disconnected
          = g_dbus_connection_is_closed(ibus_connection);
      if (disconnected) {
        // |ibus_| object is not NULL, but the connection between ibus-daemon
        // is not yet established. In this case, we don't create |ibus_config_|
        // object.
        LOG(ERROR) << "Couldn't create an ibus config object since "
                   << "IBus connection is closed.";
        return;
      }
      // If memconf is not successfully started yet, ibus_config_new() will
      // return NULL. Otherwise, it returns a transfer-none and non-floating
      // object. ibus_config_new() sometimes issues a D-Bus *synchronous* IPC
      // to check if the org.freedesktop.IBus.Config service is available.
      ibus_config_ = ibus_config_new(ibus_connection,
                                     NULL /* do not cancel the operation */,
                                     NULL /* do not get error information */);
      if (!ibus_config_) {
        LOG(ERROR) << "ibus_config_new() failed. ibus-memconf is not ready?";
        return;
      }

      // TODO(yusukes): g_object_weak_ref might be better since it allows
      // libcros to detect the delivery of the "destroy" glib signal the
      // |ibus_config_| object.
      g_object_ref(ibus_config_);
      VLOG(1) << "ibus_config_ is ready.";
    }
  }

  // Destroys IBusConfig object if |ibus_| connection is not ready. This
  // function does nothing if |ibus_config_| is NULL or |ibus_| connection is
  // still alive. Note that the IBusConfig object can't be used when |ibus_|
  // connection is not ready.
  void MaybeDestroyIBusConfig() {
    if (!ibus_) {
      LOG(ERROR) << "MaybeDestroyIBusConfig: ibus_ is NULL";
      return;
    }
    if (ibus_config_ && !ibus_bus_is_connected(ibus_)) {
      g_object_unref(ibus_config_);
      ibus_config_ = NULL;
    }
  }

  // Handles "RegisterProperties" signal from chromeos_input_method_ui.
  void DoRegisterProperties(IBusPropList* ibus_prop_list) {
    VLOG(1) << "RegisterProperties" << (ibus_prop_list ? "" : " (clear)");

    ImePropertyList prop_list;  // our representation.
    if (ibus_prop_list) {
      // You can call
      //   LOG(INFO) << "\n" << PrintPropList(ibus_prop_list, 0);
      // here to dump |ibus_prop_list|.
      if (!FlattenPropertyList(ibus_prop_list, &prop_list)) {
        // Clear properties on errors.
        DoRegisterProperties(NULL);
        return;
      }
    }
    VLOG(1) << "RegisterProperties" << (ibus_prop_list ? "" : " (clear)");
    // Notify the change.
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnRegisterImeProperties(prop_list));
  }

  // Retrieves input method status and notifies them to the UI.
  // |current_global_engine_id| is the current global engine id such as "mozc"
  // and "xkb:us::eng". If the id is unknown, an empty string "" can be passed.
  // Warning: you can call this function only from ibus callback functions
  // like FocusIn(). See http://crosbug.com/5217#c9 for details.
  void UpdateUI(const char* current_global_engine_id) {
    DCHECK(current_global_engine_id);

    const IBusEngineInfo* engine_info = NULL;
    for (size_t i = 0; i < arraysize(kIBusEngines); ++i) {
      if (kIBusEngines[i].input_method_id ==
          std::string(current_global_engine_id)) {
        engine_info = &kIBusEngines[i];
        break;
      }
    }

    InputMethodDescriptor current_input_method;
    if (engine_info) {
      current_input_method = CreateInputMethodDescriptor(
          engine_info->input_method_id,
          engine_info->xkb_layout_id,
          engine_info->language_code);
    } else {
      if (!InputMethodManager::GetInstance()->GetExtraDescriptor(
          current_global_engine_id, &current_input_method)) {
        LOG(ERROR) << current_global_engine_id
                   << " is not found in the input method white-list.";
        return;
      }
    }


    VLOG(1) << "Updating the UI. ID:" << current_input_method.id()
            << ", keyboard_layout:" << current_input_method.keyboard_layout();

    // Notify the change to update UI.
    FOR_EACH_OBSERVER(Observer, observers_,
                      OnCurrentInputMethodChanged(current_input_method));
  }

  // Installs gobject signal handlers to |ibus_|.
  void ConnectIBusSignals() {
    if (!ibus_) {
      return;
    }

    // We use g_signal_connect_after here since the callback should be called
    // *after* the IBusBusDisconnectedCallback in chromeos_input_method_ui.cc
    // is called. chromeos_input_method_ui.cc attaches the panel service object
    // to |ibus_|, and the callback in this file use the attached object.
    g_signal_connect_after(ibus_,
                           "connected",
                           G_CALLBACK(IBusBusConnectedThunk),
                           this);

    g_signal_connect(ibus_,
                     "disconnected",
                     G_CALLBACK(IBusBusDisconnectedThunk),
                     this);
    g_signal_connect(ibus_,
                     "global-engine-changed",
                     G_CALLBACK(IBusBusGlobalEngineChangedThunk),
                     this);
    g_signal_connect(ibus_,
                     "name-owner-changed",
                     G_CALLBACK(IBusBusNameOwnerChangedThunk),
                     this);
  }

  // Installs gobject signal handlers to the panel service.
  void ConnectPanelServiceSignals() {
    if (!ibus_) {
      return;
    }

    IBusPanelService* ibus_panel_service = IBUS_PANEL_SERVICE(
        g_object_get_data(G_OBJECT(ibus_), kPanelObjectKey));
    if (!ibus_panel_service) {
      LOG(ERROR) << "IBusPanelService is NOT available.";
      return;
    }
    // We don't _ref() or _weak_ref() the panel service object, since we're not
    // interested in the life time of the object.

    g_signal_connect(ibus_panel_service,
                     "focus-in",
                     G_CALLBACK(FocusInThunk),
                     this);
    g_signal_connect(ibus_panel_service,
                     "register-properties",
                     G_CALLBACK(RegisterPropertiesThunk),
                     this);
    g_signal_connect(ibus_panel_service,
                     "update-property",
                     G_CALLBACK(UpdatePropertyThunk),
                     this);
  }

  // Handles "connected" signal from ibus-daemon.
  void IBusBusConnected(IBusBus* bus) {
    LOG(WARNING) << "IBus connection is recovered.";
    MaybeRestoreConnections();
  }

  // Handles "disconnected" signal from ibus-daemon.
  void IBusBusDisconnected(IBusBus* bus) {
    LOG(WARNING) << "IBus connection is terminated.";
    // ibus-daemon might be terminated. Since |ibus_| object will automatically
    // connect to the daemon if it restarts, we don't have to set NULL on ibus_.
    // Call MaybeDestroyIBusConfig() to set |ibus_config_| to NULL temporarily.
    MaybeDestroyIBusConfig();
    VLOG(1) << "Notifying Chrome that IBus is terminated.";
    FOR_EACH_OBSERVER(Observer, observers_, OnConnectionChange(false));
  }

  // Handles "global-engine-changed" signal from ibus-daemon.
  void IBusBusGlobalEngineChanged(IBusBus* bus, const gchar* engine_name) {
    DCHECK(engine_name);
    VLOG(1) << "Global engine is changed to " << engine_name;
    UpdateUI(engine_name);
  }

  // Handles "name-owner-changed" signal from ibus-daemon. The signal is sent
  // to libcros when an IBus component such as ibus-memconf, ibus-engine-*, ..
  // is started.
  void IBusBusNameOwnerChanged(IBusBus* bus,
                               const gchar* name,
                               const gchar* old_name,
                               const gchar* new_name) {
    DCHECK(name);
    DCHECK(old_name);
    DCHECK(new_name);
    VLOG(1) << "Name owner is changed: name=" << name
            << ", old_name=" << old_name << ", new_name=" << new_name;

    if (name != std::string("org.freedesktop.IBus.Config")) {
      // Not a signal for ibus-memconf.
      return;
    }

    const std::string empty_string;
    if (old_name != empty_string || new_name == empty_string) {
      // ibus-memconf died?
      LOG(WARNING) << "Unexpected name owner change: name=" << name
                   << ", old_name=" << old_name << ", new_name=" << new_name;
      // TODO(yusukes): it might be nice to set |ibus_config_| to NULL and call
      // |OnConnectionChange| with false here to allow Chrome to
      // recover all input method configurations when ibus-memconf is
      // automatically restarted by ibus-daemon. Though ibus-memconf is pretty
      // stable and unlikely crashes.
      return;
    }

    VLOG(1) << "IBus config daemon is started. Recovering ibus_config_";

    // Try to recover |ibus_config_|. If the |ibus_config_| object is
    // successfully created, |OnConnectionChange| will be called to
    // notify Chrome that IBus is ready.
    MaybeRestoreConnections();
  }

  // Handles "FocusIn" signal from chromeos_input_method_ui.
  void FocusIn(IBusPanelService* panel, const gchar* input_context_path) {
    if (!input_context_path) {
      LOG(ERROR) << "NULL context passed";
    } else {
      VLOG(1) << "FocusIn: " << input_context_path;
    }
    // Remember the current ic path.
    input_context_path_ = Or(input_context_path, "");
  }

  // Handles "RegisterProperties" signal from chromeos_input_method_ui.
  void RegisterProperties(IBusPanelService* panel, IBusPropList* prop_list) {
    DoRegisterProperties(prop_list);
  }

  // Handles "UpdateProperty" signal from chromeos_input_method_ui.
  void UpdateProperty(IBusPanelService* panel, IBusProperty* ibus_prop) {
    VLOG(1) << "UpdateProperty";
    DCHECK(ibus_prop);

    // You can call
    //   LOG(INFO) << "\n" << PrintProp(ibus_prop, 0);
    // here to dump |ibus_prop|.

    ImePropertyList prop_list;  // our representation.
    if (!FlattenProperty(ibus_prop, &prop_list)) {
      // Don't update the UI on errors.
      LOG(ERROR) << "Malformed properties are detected";
      return;
    }
    // Notify the change.
    if (!prop_list.empty()) {
      FOR_EACH_OBSERVER(Observer, observers_,
                        OnUpdateImeProperty(prop_list));
    }
  }

  // Removes input methods that are not whitelisted from
  // |requested_input_methods| and stores them on |out_filtered_input_methods|.
  // TODO(yusukes): Write unittest.
  void FilterInputMethods(
      const std::vector<std::string>& requested_input_methods,
      std::vector<std::string>* out_filtered_input_methods) {
    out_filtered_input_methods->clear();
    for (size_t i = 0; i < requested_input_methods.size(); ++i) {
      const std::string& input_method = requested_input_methods[i];
      if (whitelist_.InputMethodIdIsWhitelisted(input_method.c_str())) {
        out_filtered_input_methods->push_back(input_method);
      } else {
        LOG(ERROR) << "Unsupported input method: " << input_method;
      }
    }
  }

  // Frees input method names in |engines| and the list itself. Please make sure
  // that |engines| points the head of the list.
  void FreeInputMethodNames(GList* engines) {
    if (engines) {
      for (GList* cursor = engines; cursor; cursor = g_list_next(cursor)) {
        g_object_unref(IBUS_ENGINE_DESC(cursor->data));
      }
      g_list_free(engines);
    }
  }

  // Copies input method names in |engines| to |out|.
  // TODO(yusukes): Write unittest.
  void AddInputMethodNames(const GList* engines, InputMethodDescriptors* out) {
    DCHECK(out);
    for (; engines; engines = g_list_next(engines)) {
      IBusEngineDesc* engine_desc = IBUS_ENGINE_DESC(engines->data);
      const gchar* name = ibus_engine_desc_get_name(engine_desc);
      const gchar* layout = ibus_engine_desc_get_layout(engine_desc);
      const gchar* language = ibus_engine_desc_get_language(engine_desc);
      if (whitelist_.InputMethodIdIsWhitelisted(name)) {
        out->push_back(CreateInputMethodDescriptor(name, layout, language));
        VLOG(1) << name << " (preloaded)";
      }
    }
  }

  // A callback function that will be called when ibus_config_set_value_async()
  // request is finished.
  static void SetImeConfigCallback(GObject* source_object,
                                   GAsyncResult* res,
                                   gpointer user_data) {
    IBusConfig* config = IBUS_CONFIG(user_data);
    g_return_if_fail(config);

    GError* error = NULL;
    const gboolean result =
        ibus_config_set_value_async_finish(config, res, &error);

    if (!result) {
      std::string message = "(unknown error)";
      if (error && error->message) {
        message = error->message;
      }
      LOG(ERROR) << "ibus_config_set_value_async failed: " << message;
    }

    if (error) {
      g_error_free(error);
    }
    g_object_unref(config);
  }

  // Connection to the ibus-daemon via IBus API. These objects are used to
  // call ibus-daemon's API (e.g. activate input methods, set config, ...)
  IBusBus* ibus_;
  IBusConfig* ibus_config_;

  // Current input context path.
  std::string input_context_path_;

  ObserverList<Observer> observers_;

  // An object which knows all valid input method and layout IDs.
  InputMethodWhitelist whitelist_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerImpl);
};

#endif  // defined(HAVE_IBUS)

// The stub implementation is used if IBus is not present.
//
// Note that this class is intentionally built even if HAVE_IBUS is
// defined so that we can easily tell build breakage when we change the
// IBusControllerImpl but forget to update the stub implementation.
class IBusControllerStubImpl : public IBusController {
 public:
  IBusControllerStubImpl() {
  }

  virtual void Connect() {
  };

  virtual void AddObserver(Observer* observer) {
  }

  virtual void RemoveObserver(Observer* observer) {
  }

  virtual bool StopInputMethodProcess() {
    return true;
  }

  virtual bool ChangeInputMethod(const std::string& name) {
    return true;
  }

  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) {
  }

  virtual bool SetImeConfig(const std::string& section,
                            const std::string& config_name,
                            const ImeConfigValue& value) {
    return true;
  }

  virtual void SendHandwritingStroke(const HandwritingStroke& stroke) {
  }

  virtual void CancelHandwriting(int n_strokes) {
  }

  // This is for ibus_controller_unittest.cc. Since the test is usually compiled
  // without HAVE_IBUS, we have to provide the same implementation as
  // IBusControllerImpl to test the whitelist class.
  virtual bool InputMethodIdIsWhitelisted(const std::string& input_method_id) {
    return whitelist_.InputMethodIdIsWhitelisted(input_method_id);
  }
  // See the comment above. We have to keep the implementation the same as
  // IBusControllerImpl.
  virtual bool XkbLayoutIsSupported(const std::string& xkb_layout) {
    return whitelist_.XkbLayoutIsSupported(xkb_layout);
  }
  // See the comment above. We have to keep the implementation the same as
  // IBusControllerImpl.
  virtual InputMethodDescriptor CreateInputMethodDescriptor(
      const std::string& id,
      const std::string& raw_layout,
      const std::string& language_code) {
    return InputMethodDescriptor(whitelist_, id, raw_layout, language_code);
  }
  // See the comment above. We have to keep the implementation the same as
  // IBusControllerImpl.
  virtual InputMethodDescriptors* GetSupportedInputMethods() {
    return GetSupportedInputMethodsInternal(whitelist_);
  }

 private:
  InputMethodWhitelist whitelist_;

  DISALLOW_COPY_AND_ASSIGN(IBusControllerStubImpl);
};

IBusController* IBusController::Create() {
#if defined(HAVE_IBUS)
  return new IBusControllerImpl;
#else
  return new IBusControllerStubImpl;
#endif
}

IBusController::~IBusController() {
}

}  // namespace input_method
}  // namespace chromeos
