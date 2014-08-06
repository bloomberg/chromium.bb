// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_INTERFACE_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_INTERFACE_H_

#include <string>
#include <vector>

#include "ui/base/ime/chromeos/ime_bridge.h"

class GURL;

namespace chromeos {

namespace input_method {
class InputMethodDescriptor;
struct KeyEventHandle;
}  // namespace input_method

// InputMethodEngine is used to translate from the Chrome IME API to the native
// API.
class InputMethodEngineInterface : public IMEEngineHandlerInterface {
 public:
  struct KeyboardEvent {
    KeyboardEvent();
    virtual ~KeyboardEvent();

    std::string type;
    std::string key;
    std::string code;
    int key_code; // only used by on-screen keyboards.
    std::string extension_id;
    bool alt_key;
    bool ctrl_key;
    bool shift_key;
    bool caps_lock;
  };

  enum {
    MENU_ITEM_MODIFIED_LABEL        = 0x0001,
    MENU_ITEM_MODIFIED_STYLE        = 0x0002,
    MENU_ITEM_MODIFIED_VISIBLE      = 0x0004,
    MENU_ITEM_MODIFIED_ENABLED      = 0x0008,
    MENU_ITEM_MODIFIED_CHECKED      = 0x0010,
    MENU_ITEM_MODIFIED_ICON         = 0x0020,
  };

  enum MenuItemStyle {
    MENU_ITEM_STYLE_NONE,
    MENU_ITEM_STYLE_CHECK,
    MENU_ITEM_STYLE_RADIO,
    MENU_ITEM_STYLE_SEPARATOR,
  };

  enum MouseButtonEvent {
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
  };

  enum SegmentStyle {
    SEGMENT_STYLE_UNDERLINE,
    SEGMENT_STYLE_DOUBLE_UNDERLINE,
  };

  enum CandidateWindowPosition {
    WINDOW_POS_CURSOR,
    WINDOW_POS_COMPOSITTION,
  };

  struct MenuItem {
    MenuItem();
    virtual ~MenuItem();

    std::string id;
    std::string label;
    MenuItemStyle style;
    bool visible;
    bool enabled;
    bool checked;

    unsigned int modified;
    std::vector<MenuItem> children;
  };

  struct InputContext {
    int id;
    std::string type;
  };

  struct UsageEntry {
    std::string title;
    std::string body;
  };

  struct Candidate {
    Candidate();
    virtual ~Candidate();

    std::string value;
    int id;
    std::string label;
    std::string annotation;
    UsageEntry usage;
    std::vector<Candidate> candidates;
  };

  struct CandidateWindowProperty {
    CandidateWindowProperty();
    virtual ~CandidateWindowProperty();
    int page_size;
    bool is_cursor_visible;
    bool is_vertical;
    bool show_window_at_composition;

    // Auxiliary text is typically displayed in the footer of the candidate
    // window.
    std::string auxiliary_text;
    bool is_auxiliary_text_visible;
  };

  struct SegmentInfo {
    int start;
    int end;
    SegmentStyle style;
  };

  class Observer {
   public:
    virtual ~Observer();

    // Called when the IME becomes the active IME.
    virtual void OnActivate(const std::string& engine_id) = 0;

    // Called when the IME is no longer active.
    virtual void OnDeactivated(const std::string& engine_id) = 0;

    // Called when a text field gains focus, and will be sending key events.
    virtual void OnFocus(const InputContext& context) = 0;

    // Called when a text field loses focus, and will no longer generate events.
    virtual void OnBlur(int context_id) = 0;

    // Called when an InputContext's properties change while it is focused.
    virtual void OnInputContextUpdate(const InputContext& context) = 0;

    // Called when the user pressed a key with a text field focused.
    virtual void OnKeyEvent(const std::string& engine_id,
                            const KeyboardEvent& event,
                            input_method::KeyEventHandle* key_data) = 0;

    // Called when the user clicks on an item in the candidate list.
    virtual void OnCandidateClicked(const std::string& engine_id,
                                    int candidate_id,
                                    MouseButtonEvent button) = 0;

    // Called when a menu item for this IME is interacted with.
    virtual void OnMenuItemActivated(const std::string& engine_id,
                                     const std::string& menu_id) = 0;

    // Called when a surrounding text is changed.
    virtual void OnSurroundingTextChanged(const std::string& engine_id,
                                          const std::string& text,
                                          int cursor_pos,
                                          int anchor_pos) = 0;

    // Called when Chrome terminates on-going text input session.
    virtual void OnReset(const std::string& engine_id) = 0;
  };

  virtual ~InputMethodEngineInterface() {}

  // Set the current composition and associated properties.
  virtual bool SetComposition(int context_id,
                              const char* text,
                              int selection_start,
                              int selection_end,
                              int cursor,
                              const std::vector<SegmentInfo>& segments,
                              std::string* error) = 0;

  // Clear the current composition.
  virtual bool ClearComposition(int context_id, std::string* error) = 0;

  // Commit the specified text to the specified context.  Fails if the context
  // is not focused.
  virtual bool CommitText(int context_id, const char* text,
                          std::string* error) = 0;

  // Send the sequence of key events.
  virtual bool SendKeyEvents(int context_id,
                             const std::vector<KeyboardEvent>& events) = 0;

  // This function returns the current property of the candidate window.
  // The caller can use the returned value as the default property and
  // modify some of specified items.
  virtual const CandidateWindowProperty&
    GetCandidateWindowProperty() const = 0;

  // Change the property of the candidate window and repaint the candidate
  // window widget.
  virtual void SetCandidateWindowProperty(
      const CandidateWindowProperty& property) = 0;

  // Show or hide the candidate window.
  virtual bool SetCandidateWindowVisible(bool visible, std::string* error) = 0;

  // Set the list of entries displayed in the candidate window.
  virtual bool SetCandidates(int context_id,
                             const std::vector<Candidate>& candidates,
                             std::string* error) = 0;

  // Set the position of the cursor in the candidate window.
  virtual bool SetCursorPosition(int context_id, int candidate_id,
                                 std::string* error) = 0;

  // Set the list of items that appears in the language menu when this IME is
  // active.
  virtual bool SetMenuItems(const std::vector<MenuItem>& items) = 0;

  // Update the state of the menu items.
  virtual bool UpdateMenuItems(const std::vector<MenuItem>& items) = 0;

  // Returns true if this IME is active, false if not.
  virtual bool IsActive() const = 0;

  // Returns the current active input_component id.
  virtual const std::string& GetActiveComponentId() const = 0;

  // Deletes |number_of_chars| unicode characters as the basis of |offset| from
  // the surrounding text. The |offset| is relative position based on current
  // caret.
  // NOTE: Currently we are falling back to backspace forwarding workaround,
  // because delete_surrounding_text is not supported in Chrome. So this
  // function is restricted for only preceding text.
  // TODO(nona): Support full spec delete surrounding text.
  virtual bool DeleteSurroundingText(int context_id,
                                     int offset,
                                     size_t number_of_chars,
                                     std::string* error) = 0;

  // Hides the input view window (from API call).
  virtual void HideInputView() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_ENGINE_INTERFACE_H_
