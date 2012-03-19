// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"  // DCHECK
#include "base/string_split.h"
#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"

namespace chromeos {
namespace input_method {

struct InputMethodConfigValue;
struct InputMethodProperty;
typedef std::vector<InputMethodProperty> InputMethodPropertyList;
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
    virtual void OnRegisterImeProperties(
        const InputMethodPropertyList& prop_list) = 0;

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
    virtual void OnUpdateImeProperty(
        const InputMethodPropertyList& prop_list) = 0;

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
  virtual bool SetInputMethodConfig(const std::string& section,
                                    const std::string& config_name,
                                    const InputMethodConfigValue& value) = 0;

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

  // Creates an InputMethodDescriptor object. |raw_layout| is a comma-separated
  // list of XKB and virtual keyboard layouts.
  // (e.g. "special-us-virtual-keyboard-for-the-input-method,us")
  virtual InputMethodDescriptor CreateInputMethodDescriptor(
      const std::string& id,
      const std::string& name,
      const std::string& raw_layout,
      const std::string& language_code) = 0;
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_IBUS_CONTROLLER_H_
