// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Rename this file to ime_bridge.h

#ifndef CHROMEOS_IME_IBUS_BRIDGE_H_
#define CHROMEOS_IME_IBUS_BRIDGE_H_

#include <string>
#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/ime/ime_constants.h"
#include "chromeos/ime/input_method_property.h"

namespace chromeos {
namespace input_method {
class CandidateWindow;
}  // namespace input_method

class IBusText;

class CHROMEOS_EXPORT IBusInputContextHandlerInterface {
 public:
  // Called when the engine commit a text.
  virtual void CommitText(const std::string& text) = 0;

  // Called when the engine forward a key event.
  virtual void ForwardKeyEvent(uint32 keyval, uint32 keycode, uint32 state) = 0;

  // Called when the engine update preedit stroing.
  virtual void UpdatePreeditText(const IBusText& text,
                                 uint32 cursor_pos,
                                 bool visible) = 0;

  // Called when the engine request showing preedit string.
  virtual void ShowPreeditText() = 0;

  // Called when the engine request hiding preedit string.
  virtual void HidePreeditText() = 0;

  // Called when the engine request deleting surrounding string.
  virtual void DeleteSurroundingText(int32 offset, uint32 length) = 0;
};


// A interface to handle the engine handler method call.
class CHROMEOS_EXPORT IBusEngineHandlerInterface {
 public:
  typedef base::Callback<void (bool consumed)> KeyEventDoneCallback;

  // Following capability mask is introduced from
  // http://ibus.googlecode.com/svn/docs/ibus-1.4/ibus-ibustypes.html#IBusCapabilite
  // TODO(nona): Move to ibus_contants and merge one in ui/base/ime/*
  enum IBusCapability {
    IBUS_CAPABILITY_PREEDIT_TEXT = 1U,
    IBUS_CAPABILITY_FOCUS = 8U,
  };

  virtual ~IBusEngineHandlerInterface() {}

  // Called when the Chrome input field get the focus.
  virtual void FocusIn(ibus::TextInputType text_input_type) = 0;

  // Called when the Chrome input field lose the focus.
  virtual void FocusOut() = 0;

  // Called when the IME is enabled.
  virtual void Enable() = 0;

  // Called when the IME is disabled.
  virtual void Disable() = 0;

  // Called when a property is activated or changed.
  virtual void PropertyActivate(const std::string& property_name) = 0;

  // Called when a property is shown.
  virtual void PropertyShow(const std::string& property_name) = 0;

  // Called when a property is hidden.
  virtual void PropertyHide(const std::string& property_name) = 0;

  // Called when the Chrome input field set their capabilities.
  virtual void SetCapability(IBusCapability capability) = 0;

  // Called when the IME is reset.
  virtual void Reset() = 0;

  // Called when the key event is received. The |keycode| is raw layout
  // independent keycode. The |keysym| is result of XLookupString function
  // which translate |keycode| to keyboard layout dependent symbol value.
  // Actual implementation must call |callback| after key event handling.
  // For example: key press event for 'd' key on us layout and dvorak layout.
  //                  keyval keycode state
  //      us layout :  0x64   0x20    0x00
  //  dvorak layout :  0x65   0x20    0x00
  virtual void ProcessKeyEvent(uint32 keysym, uint32 keycode, uint32 state,
                               const KeyEventDoneCallback& callback) = 0;

  // Called when the candidate in lookup table is clicked. The |index| is 0
  // based candidate index in lookup table. The |state| is same value as
  // GdkModifierType in
  // http://developer.gnome.org/gdk/stable/gdk-Windows.html#GdkModifierType
  virtual void CandidateClicked(uint32 index, ibus::IBusMouseButton button,
                                uint32 state) = 0;

  // Called when a new surrounding text is set. The |text| is surrounding text
  // and |cursor_pos| is 0 based index of cursor position in |text|. If there is
  // selection range, |anchor_pos| represents opposite index from |cursor_pos|.
  // Otherwise |anchor_pos| is equal to |cursor_pos|.
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) = 0;

 protected:
  IBusEngineHandlerInterface() {}
};

// A interface to handle the candidate window related method call.
class CHROMEOS_EXPORT IBusPanelCandidateWindowHandlerInterface {
 public:
  virtual ~IBusPanelCandidateWindowHandlerInterface() {}

  // Called when the IME updates the lookup table.
  virtual void UpdateLookupTable(
      const input_method::CandidateWindow& candidate_window,
      bool visible) = 0;

  // Called when the IME hides the lookup table.
  virtual void HideLookupTable() = 0;

  // Called when the IME updates the auxiliary text. The |text| is given in
  // UTF-8 encoding.
  virtual void UpdateAuxiliaryText(const std::string& text, bool visible) = 0;

  // Called when the IME hides the auxiliary text.
  virtual void HideAuxiliaryText() = 0;

  // Called when the IME updates the preedit text. The |text| is given in
  // UTF-8 encoding.
  virtual void UpdatePreeditText(const std::string& text, uint32 cursor_pos,
                                 bool visible) = 0;

  // Called when the IME hides the preedit text.
  virtual void HidePreeditText() = 0;

  // Called when the application changes its caret location.
  virtual void SetCursorLocation(const ibus::Rect& cursor_location,
                                 const ibus::Rect& composition_head) = 0;

  // Called when the text field's focus state is changed.
  // |is_focused| is true when the text field gains the focus.
  virtual void FocusStateChanged(bool is_focused) {}

 protected:
  IBusPanelCandidateWindowHandlerInterface() {}
};

// A interface to handle the property related method call.
class CHROMEOS_EXPORT IBusPanelPropertyHandlerInterface {
 public:
  virtual ~IBusPanelPropertyHandlerInterface() {}

  // Called when a new property is registered.
  virtual void RegisterProperties(
    const chromeos::input_method::InputMethodPropertyList& properties) = 0;

 protected:
  IBusPanelPropertyHandlerInterface() {}
};


// IBusBridge provides access of each IME related handler. This class is used
// for IME implementation without ibus-daemon. The legacy ibus IME communicates
// their engine with dbus protocol, but new implementation doesn't. Instead of
// dbus communcation, new implementation calls target service(e.g. PanelService
// or EngineService) directly by using this class.
class IBusBridge {
 public:
  typedef base::Callback<void()> CreateEngineHandler;

  virtual ~IBusBridge();

  // Allocates the global instance. Must be called before any calls to Get().
  static CHROMEOS_EXPORT void Initialize();

  // Releases the global instance.
  static CHROMEOS_EXPORT void Shutdown();

  // Returns IBusBridge global instance. Initialize() must be called first.
  static CHROMEOS_EXPORT IBusBridge* Get();

  // Returns current InputContextHandler. This function returns NULL if input
  // context is not ready to use.
  virtual IBusInputContextHandlerInterface* GetInputContextHandler() const = 0;

  // Updates current InputContextHandler. If there is no active input context,
  // pass NULL for |handler|. Caller must release |handler|.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) = 0;

  // Returns current EngineHandler. This function returns NULL if current engine
  // is not ready to use.
  virtual IBusEngineHandlerInterface* GetEngineHandler() const = 0;

  // Updates current EngineHandler. If there is no active engine service, pass
  // NULL for |handler|. Caller must release |handler|.
  virtual void SetEngineHandler(IBusEngineHandlerInterface* handler) = 0;

  // Returns current CandidateWindowHandler. This function returns NULL if
  // current candidate window is not ready to use.
  virtual IBusPanelCandidateWindowHandlerInterface*
      GetCandidateWindowHandler() const = 0;

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass NULL for |handler|. Caller must release |handler|.
  virtual void SetCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) = 0;

  // Returns current PropertyHandler. This function returns NULL if panel window
  // is not ready to use.
  virtual IBusPanelPropertyHandlerInterface* GetPropertyHandler() const = 0;

  // Updates current PropertyHandler. If there is no active property service,
  // pass NULL for |handler|. Caller must release |handler|.
  virtual void SetPropertyHandler(
      IBusPanelPropertyHandlerInterface* handler) = 0;

  // Sets create engine handler for |engine_id|. |engine_id| must not be empty
  // and |handler| must not be null.
  virtual void SetCreateEngineHandler(
      const std::string& engine_id,
      const CreateEngineHandler& handler) = 0;

  // Unsets create engine handler for |engine_id|. |engine_id| must not be
  // empty.
  virtual void UnsetCreateEngineHandler(const std::string& engine_id) = 0;

  // Creates engine. Do not call this function before SetCreateEngineHandler
  // call with |engine_id|.
  virtual void CreateEngine(const std::string& engine_id) = 0;

 protected:
  IBusBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusBridge);
};

}  // namespace chromeos

#endif  // CHROMEOS_IME_IBUS_BRIDGE_H_
