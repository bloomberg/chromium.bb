// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_H_

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"  // DCHECK
#include "base/string_split.h"

namespace chromeos {
namespace input_method {

// A structure which represents an input method.
struct InputMethodDescriptor {
  InputMethodDescriptor();

  // Do not call this function directly. Use CreateInputMethodDescriptor().
  InputMethodDescriptor(const std::string& in_id,
                        const std::string& in_keyboard_layout,
                        const std::string& in_virtual_keyboard_layouts,
                        const std::string& in_language_code);

  ~InputMethodDescriptor();

  bool operator==(const InputMethodDescriptor& other) const {
    return (id == other.id);
  }

  // Debug print function.
  std::string ToString() const {
    std::stringstream stream;
    stream << "id=" << id
           << ", keyboard_layout=" << keyboard_layout
           << ", virtual_keyboard_layouts=" << virtual_keyboard_layouts_
           << ", language_code=" << language_code;
    return stream.str();
  }

  // TODO(yusukes): When libcros is moved to Chrome, do the following:
  // 1. Change the type of the virtual_keyboard_layouts_ variable to
  //    std::vector<std::string> and rename it back to virtual_keyboard_layouts.
  // 2. Remove the virtual_keyboard_layouts() function.
  // We can't do them right now because it will break ABI compatibility...
  std::vector<std::string> virtual_keyboard_layouts() const {
    std::vector<std::string> layout_names;
    base::SplitString(virtual_keyboard_layouts_, ',', &layout_names);
    return layout_names;
  }

  // Preferred virtual keyboard layouts for the input method. Comma separated
  // layout names in order of priority, such as "handwriting,us", could appear.
  // Note: DO NOT ACCESS THIS VARIABLE DIRECTLY. USE virtual_keyboard_layouts()
  // INSTEAD. SEE THE TODO ABOVE.
  std::string virtual_keyboard_layouts_;

  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id;
  // A preferred physical keyboard layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
  std::string keyboard_layout;
  // Language codes like "ko", "ja", "zh_CN", and "t".
  // "t" is used for languages in the "Others" category.
  std::string language_code;
};
typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

// A structure which represents a property for an input method engine. For
// details, please check a comment for the LanguageRegisterImePropertiesFunction
// typedef below.
// TODO(yusukes): Rename this struct. "InputMethodProperty" might be better?
struct ImeProperty {
  ImeProperty(const std::string& in_key,
              const std::string& in_label,
              bool in_is_selection_item,
              bool in_is_selection_item_checked,
              int in_selection_item_id);

  ImeProperty();
  ~ImeProperty();

  // Debug print function.
  std::string ToString() const {
    std::stringstream stream;
    stream << "key=" << key
           << ", label=" << label
           << ", is_selection_item=" << is_selection_item
           << ", is_selection_item_checked=" << is_selection_item_checked
           << ", selection_item_id=" << selection_item_id;
    return stream.str();
  }

  std::string key;  // A key which identifies the property. Non-empty string.
                    // (e.g. "InputMode.HalfWidthKatakana")
  // DEPRECATED: TODO(yusukes): Remove this when it's ready.
  std::string deprecated_icon_path;
  std::string label;  // A description of the property. Non-empty string.
                      // (e.g. "Switch to full punctuation mode", "Hiragana")
  bool is_selection_item;  // true if the property is a selection item.
  bool is_selection_item_checked;  // true if |is_selection_item| is true and
                                   // the selection_item is selected.
  int selection_item_id;  // A group ID (>= 0) of the selection item.
                          // kInvalidSelectionItemId if |is_selection_item| is
                          // false.
  static const int kInvalidSelectionItemId = -1;
};
typedef std::vector<ImeProperty> ImePropertyList;

// A structure which represents a value of an input method configuration item.
// This struct is used by SetImeConfig().
// TODO(yusukes): Rename this struct. "InputMethodConfigValue" might be better?
struct ImeConfigValue {
  ImeConfigValue();
  ~ImeConfigValue();

  // Debug print function.
  std::string ToString() const {
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

  enum ValueType {
    kValueTypeString = 0,
    kValueTypeInt,
    kValueTypeBool,
    kValueTypeStringList,
  };

  // A value is stored on |string_value| member if |type| is kValueTypeString.
  // The same is true for other enum values.
  ValueType type;

  std::string string_value;
  int int_value;
  bool bool_value;
  std::vector<std::string> string_list_value;
};

typedef std::vector<std::pair<double, double> > HandwritingStroke;

// IBusController is used to interact with the IBus daemon.
class IBusController {
 public:
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when current input method is changed by a user.
    virtual void  OnCurrentInputMethodChanged(
        const InputMethodDescriptor& current_input_method) = 0;

    // Called when "RegisterProperties" signal is sent from
    // ibus-daemon. The signal contains a list of properties for a
    // specific input method engine. For example, Japanese input method
    // might have the following properties:
    //
    // ----------------------------------
    //   key: InputMode.Hiragana
    //   label: Hiragana
    //   is_selection_item: true
    //   is_selection_item_checked: true
    //   selection_item_id: 1
    // ----------------------------------
    //   key: InputMode.Katakana
    //   label: Katakana
    //   is_selection_item: true
    //   is_selection_item_checked: false
    //   selection_item_id: 1
    // ----------------------------------
    //   ...
    // ----------------------------------
    virtual void OnRegisterImeProperties(const ImePropertyList& prop_list) = 0;

    // Called when "UpdateProperty" signal is sent from ibus-daemon. The
    // signal contains one or more properties which is updated
    // recently. Keys the signal contains are a subset of keys registered
    // by the "RegisterProperties" signal above. For example, Japanese
    // input method might send the following properties:
    //
    // ----------------------------------
    //   key: InputMode.Hiragana
    //   label: Hiragana
    //   is_selection_item: true
    //   is_selection_item_checked: false
    //   selection_item_id: ...
    // ----------------------------------
    //   key: InputMode.Katakana
    //   label: Katakana
    //   is_selection_item: true
    //   is_selection_item_checked: true
    //   selection_item_id: ...
    // ----------------------------------
    //
    // Note: Please do not use selection_item_ids in |prop_list|. Dummy
    //       values are filled in the field.
    virtual void OnUpdateImeProperty(const ImePropertyList& prop_list) = 0;

    // Called when ibus connects or disconnects.
    virtual void OnConnectionChange(bool connected) = 0;
  };

  // Creates an instance of the class. The constructor is unused.
  static IBusController* Create();

  virtual ~IBusController();

  // Connects to the IBus daemon.
  virtual void Connect() = 0;

  // Adds and removes observers for IBus UI notifications. Clients must be
  // sure to remove the observer before they go away. To capture the
  // initial connection change, you should add an observer before calling
  // Connect().
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Stops ibus-daemon. Returns true on success.
  virtual bool StopInputMethodProcess() = 0;

  // Changes the current input method engine to |name|. Returns true on
  // success.  Examples of names: "pinyin", "m17n:ar:kbd",
  // "xkb:us:dvorak:eng".
  virtual bool ChangeInputMethod(const std::string& name) = 0;

  // Sets whether the input method property specified by |key| is activated.
  // If |activated| is true, activates the property. If |activated| is false,
  // deactivates the property.
  // TODO(yusukes): "SetInputMethodPropertyActivated" might be better?
  virtual void SetImePropertyActivated(const std::string& key,
                                       bool activated) = 0;

  // Sets a configuration of ibus-daemon or IBus engines to |value|.
  // Returns true if the configuration is successfully set.
  //
  // To set 'panel/custom_font', |section| should be "panel", and
  // |config_name| should be "custom_font".
  // TODO(yusukes): "SetInputMethodConfig" might be better?
  virtual bool SetImeConfig(const std::string& section,
                            const std::string& config_name,
                            const ImeConfigValue& value) = 0;

  // Sends a handwriting stroke to ibus-daemon. The std::pair contains x
  // and y coordinates. (0.0, 0.0) represents the top-left corner of a
  // handwriting area, and (1.0, 1.0) does the bottom-right. For example,
  // the second stroke for U+30ED (Katakana character Ro) would be
  // something like [(0,0), (1,0), (1,1)].  stroke.size() should always be
  // >= 2 (i.e. a single dot is not allowed).
  virtual void SendHandwritingStroke(const HandwritingStroke& stroke) = 0;

  // Clears the last N handwriting strokes. Pass zero for clearing all strokes.
  // TODO(yusukes): Currently ibus-daemon only accepts 0 for |n_strokes|.
  virtual void CancelHandwriting(int n_strokes) = 0;
};

//
// FUNCTIONS BELOW ARE ONLY FOR UNIT TESTS. DO NOT USE THEM.
//
bool InputMethodIdIsWhitelisted(const std::string& input_method_id);
bool XkbLayoutIsSupported(const std::string& xkb_layout);
InputMethodDescriptor CreateInputMethodDescriptor(
    const std::string& id,
    const std::string& raw_layout,
    const std::string& language_code);

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_H_
