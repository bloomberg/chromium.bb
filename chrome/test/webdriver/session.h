// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_SESSION_H_
#define CHROME_TEST_WEBDRIVER_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/automation.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/frame_path.h"
#include "chrome/test/webdriver/web_element_id.h"
#include "ui/gfx/point.h"

class CommandLine;
class DictionaryValue;
class FilePath;
class GURL;
class ListValue;
class Value;

namespace base {
class WaitableEvent;
}

namespace gfx {
class Rect;
class Size;
}

namespace webdriver {

// A window ID and frame path combination that uniquely identifies a specific
// frame within a session.
struct FrameId {
  FrameId(int window_id, const FramePath& frame_path);
  FrameId& operator=(const FrameId& other);
  int window_id;
  FramePath frame_path;
};

// Every connection made by WebDriver maps to a session object.
// This object creates the chrome instance and keeps track of the
// state necessary to control the chrome browser created.
// A session manages its own lifetime.
class Session {
 public:
  enum Speed { kSlow, kMedium, kFast, kUnknown };

  // Adds this |Session| to the |SessionManager|. The session manages its own
  // lifetime. Do not call delete.
  Session();
  // Removes this |Session| from the |SessionManager|.
  ~Session();

  // Starts the session thread and a new browser, using the exe found at
  // |browser_exe|. If |browser_exe| is empty, it will search in all the default
  // locations. Returns true on success. On failure, the session will delete
  // itself and return an error code.
  ErrorCode Init(const FilePath& browser_exe,
                 const CommandLine& options);

  // Terminates this session and deletes itself.
  void Terminate();

  // Executes the given |script| in the context of the given frame.
  // The |script| should be in the form of a function body
  // (e.g. "return arguments[0]"), where |args| is the list of arguments to
  // pass to the function. The caller is responsible for the script result
  // |value|.
  ErrorCode ExecuteScript(const FrameId& frame_id,
                          const std::string& script,
                          const ListValue* const args,
                          Value** value);

  // Same as above, but uses the currently targeted window and frame.
  ErrorCode ExecuteScript(const std::string& script,
                          const ListValue* const args,
                          Value** value);

  // Executes given |script| in the context of the given frame.
  // The |script| should be in the form of a function body
  // (e.g. "return arguments[0]"), where |args| is the list of arguments to
  // pass to the function. The caller is responsible for the script result
  // |value|.
  ErrorCode ExecuteAsyncScript(const FrameId& frame_id,
                               const std::string& script,
                               const ListValue* const args,
                               Value** value);

  // Send the given keys to the given element dictionary. This function takes
  // ownership of |element|.
  ErrorCode SendKeys(const WebElementId& element, const string16& keys);

  // Clicks the mouse at the given location using the given button.
  bool MouseMoveAndClick(const gfx::Point& location,
                         automation::MouseButton button);
  bool MouseMove(const gfx::Point& location);
  bool MouseDrag(const gfx::Point& start, const gfx::Point& end);
  bool MouseClick(automation::MouseButton button);
  bool MouseButtonDown();
  bool MouseButtonUp();
  bool MouseDoubleClick();

  bool NavigateToURL(const std::string& url);
  bool GoForward();
  bool GoBack();
  bool Reload();
  ErrorCode GetURL(std::string* url);
  ErrorCode GetURL(GURL* url);
  ErrorCode GetTitle(std::string* tab_title);
  bool GetScreenShot(std::string* png);

  bool GetCookies(const std::string& url, ListValue** cookies);
  bool GetCookiesDeprecated(const GURL& url, std::string* cookies);
  bool GetCookieByNameDeprecated(const GURL& url,
                                 const std::string& cookie_name,
                                 std::string* cookie);
  bool DeleteCookie(const std::string& url, const std::string& cookie_name);
  bool DeleteCookieDeprecated(const GURL& url, const std::string& cookie_name);
  bool SetCookie(const std::string& url, DictionaryValue* cookie_dict);
  bool SetCookieDeprecated(const GURL& url, const std::string& cookie);

  // Gets all the currently existing window IDs. Returns true on success.
  bool GetWindowIds(std::vector<int>* window_ids);

  // Switches the window used by default. |name| is either an ID returned by
  // |GetWindowIds| or the name attribute of a DOM window.
  ErrorCode SwitchToWindow(const std::string& name);

  // Switches the frame used by default. |name_or_id| is either the name or id
  // of a frame element.
  ErrorCode SwitchToFrameWithNameOrId(const std::string& name_or_id);

  // Switches the frame used by default. |index| is the zero-based frame index.
  ErrorCode SwitchToFrameWithIndex(int index);

  // Switches to the frame identified by the given |element|. The element must
  // be either an IFRAME or FRAME element.
  ErrorCode SwitchToFrameWithElement(const WebElementId& element);

  // Switches the target frame to the topmost frame.
  void SwitchToTopFrame();

  // Switches the target frame to the topmost frame if the current frame is
  // invalid.
  void SwitchToTopFrameIfCurrentFrameInvalid();

  // Closes the current window. Returns true on success.
  // Note: The session will be deleted if this closes the last window in the
  // session.
  bool CloseWindow();

  // Gets the message of the currently active JavaScript modal dialog.
  ErrorCode GetAlertMessage(std::string* text);

  // Sets the prompt text to use when accepting or dismissing a JavaScript
  // modal dialog.
  ErrorCode SetAlertPromptText(const std::string& alert_prompt_text);

  // Accept or dismiss the currently active JavaScript modal dialog with the
  // previously set alert prompt text. Then clears the saved alert prompt text.
  ErrorCode AcceptOrDismissAlert(bool accept);

  // Gets the version of the running browser.
  std::string GetBrowserVersion();

  // Gets whether the running browser's version is newer or equal to the given
  // version. Returns true on successful comparison. For example, in the version
  // 11.0.632.4, 632 is the build number and 4 is the patch number.
  bool CompareBrowserVersion(int build_no,
                             int patch_no,
                             bool* is_newer_or_equal);

  // Finds a single element in the given frame, starting at the given
  // |root_element|, using the given locator strategy. |locator| should be a
  // constant from |LocatorType|. Returns an error code. If successful,
  // |element| will be set as the found element.
  ErrorCode FindElement(const FrameId& frame_id,
                        const WebElementId& root_element,
                        const std::string& locator,
                        const std::string& query,
                        WebElementId* element);

  // Same as above, but finds multiple elements.
  ErrorCode FindElements(const FrameId& frame_id,
                         const WebElementId& root_element,
                         const std::string& locator,
                         const std::string& query,
                         std::vector<WebElementId>* elements);

  // Scroll the element into view and get its location relative to the client's
  // viewport.
  ErrorCode GetElementLocationInView(
      const WebElementId& element, gfx::Point* location);

  // Gets the size of the element from the given window and frame, even if
  // its display is none.
  ErrorCode GetElementSize(const FrameId& frame_id,
                           const WebElementId& element,
                           gfx::Size* size);

  // Gets the element's effective style for the given property.
  ErrorCode GetElementEffectiveStyle(
      const FrameId& frame_id,
      const WebElementId& element,
      const std::string& prop,
      std::string* value);

  // Gets the top and left element border widths for the given frame.
  ErrorCode GetElementBorder(const FrameId& frame_id,
                             const WebElementId& element,
                             int* border_left,
                             int* border_top);

  // Gets whether the element is currently displayed.
  ErrorCode IsElementDisplayed(const FrameId& frame_id,
                               const WebElementId& element,
                               bool* is_visible);

  // Gets whether the element is currently enabled.
  ErrorCode IsElementEnabled(const FrameId& frame_id,
                             const WebElementId& element,
                             bool* is_enabled);

  // Waits for all tabs to stop loading. Returns true on success.
  bool WaitForAllTabsToStopLoading();

  const std::string& id() const;

  const FrameId& current_target() const;

  void set_async_script_timeout(int timeout_ms);
  int async_script_timeout() const;

  void set_implicit_wait(int timeout_ms);
  int implicit_wait() const;

  void set_speed(Speed speed);
  Speed speed() const;

  void set_screenshot_on_error(bool error);
  bool screenshot_on_error() const;

  void set_use_native_events(bool use_native_events);
  bool use_native_events() const;

  const gfx::Point& get_mouse_position() const;

 private:
  void RunSessionTask(Task* task);
  void RunSessionTaskOnSessionThread(
      Task* task,
      base::WaitableEvent* done_event);
  void InitOnSessionThread(const FilePath& browser_exe,
                           const CommandLine& options,
                           ErrorCode* code);
  void TerminateOnSessionThread();

  // Executes the given |script| in the context of the given frame.
  // Waits for script to finish and parses the response.
  // The caller is responsible for the script result |value|.
  ErrorCode ExecuteScriptAndParseResponse(const FrameId& frame_id,
                                          const std::string& script,
                                          Value** value);

  void SendKeysOnSessionThread(const string16& keys, bool* success);
  ErrorCode SwitchToFrameWithJavaScriptLocatedFrame(
      const std::string& script,
      ListValue* args);
  ErrorCode FindElementsHelper(const FrameId& frame_id,
                               const WebElementId& root_element,
                               const std::string& locator,
                               const std::string& query,
                               bool find_one,
                               std::vector<WebElementId>* elements);
  ErrorCode GetLocationInViewHelper(const FrameId& frame_id,
                                    const WebElementId& element,
                                    const gfx::Rect& region,
                                    gfx::Point* location);

  const std::string id_;
  FrameId current_target_;

  scoped_ptr<Automation> automation_;
  base::Thread thread_;

  // Timeout (in ms) for asynchronous script execution.
  int async_script_timeout_;

  // Time (in ms) of how long to wait while searching for a single element.
  int implicit_wait_;

  Speed speed_;

  // Since screenshots can be very large when in base64 PNG format; the
  // client is allowed to dyamically enable/disable screenshots on error
  // during the lifetime of the session.
  bool screenshot_on_error_;

  // True if the session should simulate OS-level input. Currently only applies
  // to keyboard input.
  bool use_native_events_;

  // Vector of the |WebElementId|s for each frame of the current target frame
  // path. The first refers to the first frame element in the root document.
  // If the target frame is window.top, this will be empty.
  std::vector<WebElementId> frame_elements_;

  // Last mouse position. Advanced APIs need this value.
  gfx::Point mouse_position_;

  // Chrome does not have an individual method for setting the prompt text
  // of an alert. Instead, when the WebDriver client wants to set the text,
  // we store it here and pass the text when the alert is accepted or
  // dismissed. This text should only be used if |has_alert_prompt_text_|
  // is true, so that the default prompt text is not overridden.
  std::string alert_prompt_text_;
  bool has_alert_prompt_text_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Session);

#endif  // CHROME_TEST_WEBDRIVER_SESSION_H_
