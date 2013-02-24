// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_SESSION_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/threading/thread.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/automation/automation_json_requests.h"
#include "chrome/test/webdriver/frame_path.h"
#include "chrome/test/webdriver/webdriver_automation.h"
#include "chrome/test/webdriver/webdriver_basic_types.h"
#include "chrome/test/webdriver/webdriver_capabilities_parser.h"
#include "chrome/test/webdriver/webdriver_element_id.h"
#include "chrome/test/webdriver/webdriver_logging.h"

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
class Value;
class WaitableEvent;
}

namespace webdriver {

class Error;
class ValueParser;

// A view ID and frame path combination that uniquely identifies a specific
// frame within a session.
struct FrameId {
  FrameId();
  FrameId(const WebViewId& view_id, const FramePath& frame_path);

  WebViewId view_id;
  FramePath frame_path;
};

enum StorageType {
  kLocalStorageType = 0,
  kSessionStorageType
};

// Every connection made by WebDriver maps to a session object.
// This object creates the chrome instance and keeps track of the
// state necessary to control the chrome browser created.
// A session manages its own lifetime.
class Session {
 public:
  // Adds this |Session| to the |SessionManager|. The session manages its own
  // lifetime. Call |Terminate|, not delete, if you need to quit.
  Session();

  // Removes this |Session| from the |SessionManager|.
  ~Session();

  // Initializes the session with the given capabilities.
  Error* Init(const base::DictionaryValue* capabilities_dict);

  // Should be called before executing a command.
  Error* BeforeExecuteCommand();

  // Should be called after executing a command.
  Error* AfterExecuteCommand();

  // Terminates this session and deletes itself.
  void Terminate();

  // Executes the given |script| in the context of the given frame.
  // The |script| should be in the form of a function body
  // (e.g. "return arguments[0]"), where |args| is the list of arguments to
  // pass to the function. The caller is responsible for the script result
  // |value|, which is set only if there is no error.
  Error* ExecuteScript(const FrameId& frame_id,
                       const std::string& script,
                       const base::ListValue* const args,
                       base::Value** value);

  // Same as above, but uses the currently targeted window and frame.
  Error* ExecuteScript(const std::string& script,
                       const base::ListValue* const args,
                       base::Value** value);

  // Executes the given script in the context of the given frame and parses
  // the value with the given parser. The script should be in the form of an
  // anonymous function. |script_name| is only used when creating error
  // messages. This function takes ownership of |args| and |parser|.
  Error* ExecuteScriptAndParse(const FrameId& frame_id,
                               const std::string& anonymous_func_script,
                               const std::string& script_name,
                               const base::ListValue* args,
                               const ValueParser* parser);

  // Executes given |script| in the context of the given frame.
  // The |script| should be in the form of a function body
  // (e.g. "return arguments[0]"), where |args| is the list of arguments to
  // pass to the function. The caller is responsible for the script result
  // |value|, which is set only if there is no error.
  Error* ExecuteAsyncScript(const FrameId& frame_id,
                            const std::string& script,
                            const base::ListValue* const args,
                            base::Value** value);

  // Send the given keys to the given element dictionary. This function takes
  // ownership of |element|.
  Error* SendKeys(const ElementId& element, const string16& keys);
  // Send the given keys to the active element.
  Error* SendKeys(const string16& keys);

  // Sets the file paths to the file upload control under the given location.
  Error* DragAndDropFilePaths(
      const Point& location,
      const std::vector<base::FilePath::StringType>& paths);

  // Clicks the mouse at the given location using the given button.
  Error* MouseMoveAndClick(const Point& location,
                           automation::MouseButton button);
  Error* MouseMove(const Point& location);
  Error* MouseDrag(const Point& start, const Point& end);
  Error* MouseClick(automation::MouseButton button);
  Error* MouseButtonDown();
  Error* MouseButtonUp();
  Error* MouseDoubleClick();

  Error* NavigateToURL(const std::string& url);
  Error* GoForward();
  Error* GoBack();
  Error* Reload();
  Error* GetURL(std::string* url);
  Error* GetTitle(std::string* tab_title);
  Error* GetScreenShot(std::string* png);
#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  Error* HeapProfilerDump(const std::string& reason);
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  Error* GetCookies(const std::string& url, base::ListValue** cookies);
  Error* DeleteCookie(const std::string& url, const std::string& cookie_name);
  Error* SetCookie(const std::string& url, base::DictionaryValue* cookie_dict);

  // Gets all the currently open views.
  Error* GetViews(std::vector<WebViewInfo>* views);

  // Switches the view used by default. |id_or_name| is either a view ID
  // returned by |GetViews| or the name attribute of a DOM window.
  // Only tabs are considered when searching by name.
  Error* SwitchToView(const std::string& id_or_name);

  // Switches the frame used by default. |name_or_id| is either the name or id
  // of a frame element.
  Error* SwitchToFrameWithNameOrId(const std::string& name_or_id);

  // Switches the frame used by default. |index| is the zero-based frame index.
  Error* SwitchToFrameWithIndex(int index);

  // Switches to the frame identified by the given |element|. The element must
  // be either an IFRAME or FRAME element.
  Error* SwitchToFrameWithElement(const ElementId& element);

  // Switches the target frame to the topmost frame.
  void SwitchToTopFrame();

  // Switches the target frame to the topmost frame if the current frame is
  // invalid.
  Error* SwitchToTopFrameIfCurrentFrameInvalid();

  // Closes the current window. Returns true on success.
  // Note: The session will be deleted if this closes the last window in the
  // session.
  Error* CloseWindow();

  // Gets the bounds for the specified window.
  Error* GetWindowBounds(const WebViewId& window, Rect* bounds);

  // Sets the bounds for the specified window.
  Error* SetWindowBounds(const WebViewId& window, const Rect& bounds);

  // Maximizes the specified window.
  Error* MaximizeWindow(const WebViewId& window);

  // Gets the message of the currently active JavaScript modal dialog.
  Error* GetAlertMessage(std::string* text);

  // Sets the prompt text to use when accepting or dismissing a JavaScript
  // modal dialog.
  Error* SetAlertPromptText(const std::string& alert_prompt_text);

  // Accept or dismiss the currently active JavaScript modal dialog with the
  // previously set alert prompt text. Then clears the saved alert prompt text.
  Error* AcceptOrDismissAlert(bool accept);

  // Gets the version of the running browser.
  std::string GetBrowserVersion();

  // Gets whether the running browser's version is newer or equal to the given
  // version. Returns true on successful comparison. For example, in the version
  // 11.0.632.4, 632 is the build number and 4 is the patch number.
  Error* CompareBrowserVersion(int build_no,
                               int patch_no,
                               bool* is_newer_or_equal);

  // Finds a single element in the given frame, starting at the given
  // |root_element|, using the given locator strategy. |locator| should be a
  // constant from |LocatorType|. Returns an error code. If successful,
  // |element| will be set as the found element.
  Error* FindElement(const FrameId& frame_id,
                     const ElementId& root_element,
                     const std::string& locator,
                     const std::string& query,
                     ElementId* element);

  // Same as above, but finds multiple elements.
  Error* FindElements(const FrameId& frame_id,
                      const ElementId& root_element,
                      const std::string& locator,
                      const std::string& query,
                      std::vector<ElementId>* elements);

  // Scroll the element into view and get its location relative to
  // the client's viewport.
  Error* GetElementLocationInView(
      const ElementId& element,
      Point* location);

  // Scroll the element's region into view and get its location relative to
  // the client's viewport. If |center| is true, the element will be centered
  // if it is too big to fit in view. If |verify_clickable_at_middle| is true,
  // an error will be returned if the element is not clickable in the middle
  // of the given region.
  Error* GetElementRegionInView(
      const ElementId& element,
      const Rect& region,
      bool center,
      bool verify_clickable_at_middle,
      Point* location);

  // Gets the size of the element from the given window and frame, even if
  // its display is none.
  Error* GetElementSize(const FrameId& frame_id,
                        const ElementId& element,
                        Size* size);

  // Gets the size of the element's first client rect. If the element has
  // no client rects, this will return an error.
  Error* GetElementFirstClientRect(const FrameId& frame_id,
                                   const ElementId& element,
                                   Rect* rect);

  // Gets the element's effective style for the given property.
  Error* GetElementEffectiveStyle(
      const FrameId& frame_id,
      const ElementId& element,
      const std::string& prop,
      std::string* value);

  // Gets the top and left element border widths for the given frame.
  Error* GetElementBorder(const FrameId& frame_id,
                          const ElementId& element,
                          int* border_left,
                          int* border_top);

  // Gets whether the element is currently displayed.
  Error* IsElementDisplayed(const FrameId& frame_id,
                            const ElementId& element,
                            bool ignore_opacity,
                            bool* is_visible);

  // Gets whether the element is currently enabled.
  Error* IsElementEnabled(const FrameId& frame_id,
                          const ElementId& element,
                          bool* is_enabled);

  // Gets whether the option element is currently selected.
  Error* IsOptionElementSelected(const FrameId& frame_id,
                                 const ElementId& element,
                                 bool* is_selected);

  // Set the selection state of the given option element. The option element
  // must support multi selection if |selected| is false.
  Error* SetOptionElementSelected(const FrameId& frame_id,
                                  const ElementId& element,
                                  bool selected);

  // Toggles the option element's selection state. The option element should
  // support multi selection.
  Error* ToggleOptionElement(const FrameId& frame_id,
                             const ElementId& element);

  // Gets the tag name of the given element.
  Error* GetElementTagName(const FrameId& frame_id,
                           const ElementId& element,
                           std::string* tag_name);

  // Gets the clickable location of the given element. It will be the center
  // location of the element. If the element is not clickable, or if the
  // location cannot be determined, an error will be returned.
  Error* GetClickableLocation(const ElementId& element,
                              Point* location);

  // Gets the attribute of the given element. If there are no errors, the
  // function sets |value| and the caller takes ownership.
  Error* GetAttribute(const ElementId& element, const std::string& key,
                      base::Value** value);

  // Waits for all views to stop loading. Returns true on success.
  Error* WaitForAllViewsToStopLoading();

  // Install extension at |path|.
  Error* InstallExtension(const base::FilePath& path, std::string* extension_id);

  Error* GetExtensionsInfo(base::ListValue* extension_ids);

  Error* IsPageActionVisible(const WebViewId& tab_id,
                             const std::string& extension_id,
                             bool* is_visible);

  Error* SetExtensionState(const std::string& extension_id,
                           bool enable);

  Error* ClickExtensionButton(const std::string& extension_id,
                              bool browser_action);

  Error* UninstallExtension(const std::string& extension_id);

  // Sets the preference to the given value. This function takes ownership
  // of |value|. If the preference is a user preference (instead of local
  // state preference) |is_user_pref| should be true.
  Error* SetPreference(const std::string& pref,
                       bool is_user_pref,
                       base::Value* value);

  // Returns a copy of the current log entries. Caller is responsible for
  // returned value.
  base::ListValue* GetLog() const;

  // Gets the browser connection state.
  Error* GetBrowserConnectionState(bool* online);

  // Gets the status of the application cache.
  Error* GetAppCacheStatus(int* status);

  // Sets an item in the HTML5 localStorage object.
  Error* SetStorageItem(StorageType type,
                        const std::string& key,
                        const std::string& value);

  // Gets the value of an item in the HTML5 localStorage object.
  Error* GetStorageItem(StorageType type,
                        const std::string& key,
                        std::string* value);

  // Removes an item from the HTML5 localStorage object.
  Error* RemoveStorageItem(StorageType type,
                           const std::string& key,
                           std::string* value);

  // Gets the total number of items in the HTML5 localStorage object.
  Error* GetStorageSize(StorageType type, int* size);

  // Removes all items in the HTML5 localStorage object.
  Error* ClearStorage(StorageType type);

  // Gets the keys of all items of the HTML5 localStorage object. If there are
  // no errors, the function sets |keys| and the caller takes ownership.
  Error* GetStorageKeys(StorageType type, base::ListValue** keys);

  // Gets the current geolocation.
  Error* GetGeolocation(scoped_ptr<base::DictionaryValue>* geolocation);

  // Overrides the current geolocation.
  Error* OverrideGeolocation(const base::DictionaryValue* geolocation);

  const std::string& id() const;

  const FrameId& current_target() const;

  void set_async_script_timeout(int timeout_ms);
  int async_script_timeout() const;

  void set_implicit_wait(int timeout_ms);
  int implicit_wait() const;

  const Point& get_mouse_position() const;

  const Logger& logger() const;

  const base::FilePath& temp_dir() const;

  const Capabilities& capabilities() const;

 private:
  void RunSessionTask(const base::Closure& task);
  void RunClosureOnSessionThread(
      const base::Closure& task,
      base::WaitableEvent* done_event);
  void InitOnSessionThread(const Automation::BrowserOptions& options,
                           int* build_no,
                           Error** error);
  void TerminateOnSessionThread();

  // Executes the given |script| in the context of the given frame.
  // Waits for script to finish and parses the response.
  // The caller is responsible for the script result |value|.
  Error* ExecuteScriptAndParseValue(const FrameId& frame_id,
                                    const std::string& script,
                                    base::Value** value);
  void SendKeysOnSessionThread(const string16& keys,
                               bool release_modifiers,
                               Error** error);
  Error* ProcessWebMouseEvents(const std::vector<WebMouseEvent>& events);
  WebMouseEvent CreateWebMouseEvent(automation::MouseEventType type,
                                    automation::MouseButton button,
                                    const Point& point,
                                    int click_count);
  Error* SwitchToFrameWithJavaScriptLocatedFrame(
      const std::string& script,
      base::ListValue* args);
  Error* FindElementsHelper(const FrameId& frame_id,
                            const ElementId& root_element,
                            const std::string& locator,
                            const std::string& query,
                            bool find_one,
                            std::vector<ElementId>* elements);
  Error* ExecuteFindElementScriptAndParse(const FrameId& frame_id,
                                          const ElementId& root_element,
                                          const std::string& locator,
                                          const std::string& query,
                                          bool find_one,
                                          std::vector<ElementId>* elements);
  // Returns an error if the element is not clickable.
  Error* VerifyElementIsClickable(
      const FrameId& frame_id,
      const ElementId& element,
      const Point& location);
  Error* GetElementRegionInViewHelper(
      const FrameId& frame_id,
      const ElementId& element,
      const Rect& region,
      bool center,
      bool verify_clickable_at_middle,
      Point* location);
  Error* PostBrowserStartInit();
  Error* InitForWebsiteTesting();
  Error* SetPrefs();

  scoped_ptr<InMemoryLog> session_log_;
  Logger logger_;

  const std::string id_;
  FrameId current_target_;

  scoped_ptr<Automation> automation_;
  base::Thread thread_;

  // Timeout (in ms) for asynchronous script execution.
  int async_script_timeout_;

  // Time (in ms) of how long to wait while searching for a single element.
  int implicit_wait_;

  // Vector of the |ElementId|s for each frame of the current target frame
  // path. The first refers to the first frame element in the root document.
  // If the target frame is window.top, this will be empty.
  std::vector<ElementId> frame_elements_;

  // Last mouse position. Advanced APIs need this value.
  Point mouse_position_;

  // Chrome does not have an individual method for setting the prompt text
  // of an alert. Instead, when the WebDriver client wants to set the text,
  // we store it here and pass the text when the alert is accepted or
  // dismissed. This text should only be used if |has_alert_prompt_text_|
  // is true, so that the default prompt text is not overridden.
  std::string alert_prompt_text_;
  bool has_alert_prompt_text_;

  // Temporary directory containing session data.
  base::ScopedTempDir temp_dir_;
  Capabilities capabilities_;

  // Current state of all modifier keys.
  int sticky_modifiers_;

  // Chrome's build number. This is the 3rd number in Chrome's version string
  // (e.g., 18.0.995.0 -> 995). Only valid after Chrome has started.
  // See http://dev.chromium.org/releases/version-numbers.
  int build_no_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_SESSION_H_
