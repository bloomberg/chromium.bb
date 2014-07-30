// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_RUNNER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_RUNNER_H_

#include <deque>
#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "content/shell/renderer/test_runner/web_test_runner.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

namespace blink {
class WebFrame;
class WebNotificationPresenter;
class WebPermissionClient;
class WebString;
class WebView;
}

namespace gin {
class ArrayBufferView;
class Arguments;
}

namespace content {

class InvokeCallbackTask;
class NotificationPresenter;
class TestInterfaces;
class TestPageOverlay;
class WebPermissions;
class WebTestDelegate;
class WebTestProxyBase;

class TestRunner : public WebTestRunner,
                   public base::SupportsWeakPtr<TestRunner> {
 public:
  explicit TestRunner(TestInterfaces*);
  virtual ~TestRunner();

  void Install(blink::WebFrame* frame);

  void SetDelegate(WebTestDelegate*);
  void SetWebView(blink::WebView*, WebTestProxyBase*);

  void Reset();

  WebTaskList* mutable_task_list() { return &task_list_; }

  void SetTestIsRunning(bool);
  bool TestIsRunning() const { return test_is_running_; }

  bool UseMockTheme() const { return use_mock_theme_; }

  void InvokeCallback(scoped_ptr<InvokeCallbackTask> callback);

  // WebTestRunner implementation.
  virtual bool ShouldGeneratePixelResults() OVERRIDE;
  virtual bool ShouldDumpAsAudio() const OVERRIDE;
  virtual void GetAudioData(
      std::vector<unsigned char>* buffer_view) const OVERRIDE;
  virtual bool ShouldDumpBackForwardList() const OVERRIDE;
  virtual blink::WebPermissionClient* GetWebPermissions() const OVERRIDE;

  // Methods used by WebTestProxyBase.
  bool shouldDumpSelectionRect() const;
  bool isPrinting() const;
  bool shouldDumpAsText();
  bool shouldDumpAsTextWithPixelResults();
  bool shouldDumpAsCustomText() const;
  std:: string customDumpText() const;
  bool shouldDumpAsMarkup();
  bool shouldDumpChildFrameScrollPositions() const;
  bool shouldDumpChildFramesAsMarkup() const;
  bool shouldDumpChildFramesAsText() const;
  void showDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void clearDevToolsLocalStorage();
  void setShouldDumpAsText(bool);
  void setShouldDumpAsMarkup(bool);
  void setCustomTextOutput(std::string text);
  void setShouldGeneratePixelResults(bool);
  void setShouldDumpFrameLoadCallbacks(bool);
  void setShouldDumpPingLoaderCallbacks(bool);
  void setShouldEnableViewSource(bool);
  bool shouldDumpEditingCallbacks() const;
  bool shouldDumpFrameLoadCallbacks() const;
  bool shouldDumpPingLoaderCallbacks() const;
  bool shouldDumpUserGestureInFrameLoadCallbacks() const;
  bool shouldDumpTitleChanges() const;
  bool shouldDumpIconChanges() const;
  bool shouldDumpCreateView() const;
  bool canOpenWindows() const;
  bool shouldDumpResourceLoadCallbacks() const;
  bool shouldDumpResourceRequestCallbacks() const;
  bool shouldDumpResourceResponseMIMETypes() const;
  bool shouldDumpStatusCallbacks() const;
  bool shouldDumpProgressFinishedCallback() const;
  bool shouldDumpSpellCheckCallbacks() const;
  bool shouldStayOnPageAfterHandlingBeforeUnload() const;
  bool shouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* httpHeadersToClear() const;
  void setTopLoadingFrame(blink::WebFrame*, bool);
  blink::WebFrame* topLoadingFrame() const;
  void policyDelegateDone();
  bool policyDelegateEnabled() const;
  bool policyDelegateIsPermissive() const;
  bool policyDelegateShouldNotifyDone() const;
  bool shouldInterceptPostMessage() const;
  bool shouldDumpResourcePriorities() const;
  blink::WebNotificationPresenter* notification_presenter() const;
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool isPointerLocked();
  void setToolTipText(const blink::WebString&);

  bool midiAccessorResult();

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}

    // Returns true if this started a load.
    virtual bool Run(WebTestDelegate*, blink::WebView*) = 0;
  };

 private:
  friend class InvokeCallbackTask;
  friend class TestRunnerBindings;
  friend class WorkQueue;

  // Helper class for managing events queued by methods like queueLoad or
  // queueScript.
  class WorkQueue {
   public:
    explicit WorkQueue(TestRunner* controller);
    virtual ~WorkQueue();
    void ProcessWorkSoon();

    // Reset the state of the class between tests.
    void Reset();

    void AddWork(WorkItem*);

    void set_frozen(bool frozen) { frozen_ = frozen; }
    bool is_empty() { return queue_.empty(); }
    WebTaskList* mutable_task_list() { return &task_list_; }

   private:
    void ProcessWork();

    class WorkQueueTask : public WebMethodTask<WorkQueue> {
     public:
      WorkQueueTask(WorkQueue* object) : WebMethodTask<WorkQueue>(object) {}

      virtual void runIfValid() OVERRIDE;
    };

    WebTaskList task_list_;
    std::deque<WorkItem*> queue_;
    bool frozen_;
    TestRunner* controller_;
  };

  ///////////////////////////////////////////////////////////////////////////
  // Methods dealing with the test logic

  // By default, tests end when page load is complete. These methods are used
  // to delay the completion of the test until notifyDone is called.
  void NotifyDone();
  void WaitUntilDone();

  // Methods for adding actions to the work queue. Used in conjunction with
  // waitUntilDone/notifyDone above.
  void QueueBackNavigation(int how_far_back);
  void QueueForwardNavigation(int how_far_forward);
  void QueueReload();
  void QueueLoadingScript(const std::string& script);
  void QueueNonLoadingScript(const std::string& script);
  void QueueLoad(const std::string& url, const std::string& target);
  void QueueLoadHTMLString(gin::Arguments* args);

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  void SetCustomPolicyDelegate(gin::Arguments* args);

  // Delays completion of the test until the policy delegate runs.
  void WaitForPolicyDelegate();

  // Functions for dealing with windows. By default we block all new windows.
  int WindowCount();
  void SetCloseRemainingWindowsWhenComplete(bool close_remaining_windows);
  void ResetTestHelperControllers();

  ///////////////////////////////////////////////////////////////////////////
  // Methods implemented entirely in terms of chromium's public WebKit API

  // Method that controls whether pressing Tab key cycles through page elements
  // or inserts a '\t' char in text area
  void SetTabKeyCyclesThroughElements(bool tab_key_cycles_through_elements);

  // Executes an internal command (superset of document.execCommand() commands).
  void ExecCommand(gin::Arguments* args);

  // Checks if an internal command is currently available.
  bool IsCommandEnabled(const std::string& command);

  bool CallShouldCloseOnWebView();
  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  v8::Handle<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int world_id, const std::string& script);
  void EvaluateScriptInIsolatedWorld(int world_id, const std::string& script);
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      v8::Handle<v8::Value> origin);
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const std::string& policy);

  // Allows layout tests to manage origins' whitelisting.
  void AddOriginAccessWhitelistEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);
  void RemoveOriginAccessWhitelistEntry(const std::string& source_origin,
                                        const std::string& destination_protocol,
                                        const std::string& destination_host,
                                        bool allow_destination_subdomains);

  // Returns true if the current page box has custom page size style for
  // printing.
  bool HasCustomPageSizeStyle(int page_index);

  // Forces the selection colors for testing under Linux.
  void ForceRedSelectionColors();

  // Adds a style sheet to be injected into new documents.
  void InjectStyleSheet(const std::string& source_code, bool all_frames);

  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);

  std::string SelectionAsMarkup();

  // Enables or disables subpixel positioning (i.e. fractional X positions for
  // glyphs) in text rendering on Linux. Since this method changes global
  // settings, tests that call it must use their own custom font family for
  // all text that they render. If not, an already-cached style will be used,
  // resulting in the changed setting being ignored.
  void SetTextSubpixelPositioning(bool value);

  // Switch the visibility of the page.
  void SetPageVisibility(const std::string& new_visibility);

  // Changes the direction of the focused element.
  void SetTextDirection(const std::string& direction_name);

  // After this function is called, all window-sizing machinery is
  // short-circuited inside the renderer. This mode is necessary for
  // some tests that were written before browsers had multi-process architecture
  // and rely on window resizes to happen synchronously.
  // The function has "unfortunate" it its name because we must strive to remove
  // all tests that rely on this... well, unfortunate behavior. See
  // http://crbug.com/309760 for the plan.
  void UseUnfortunateSynchronousResizeMode();

  bool EnableAutoResizeMode(int min_width,
                            int min_height,
                            int max_width,
                            int max_height);
  bool DisableAutoResizeMode(int new_width, int new_height);

  void SetMockDeviceLight(double value);
  void ResetDeviceLight();
  // Device Motion / Device Orientation related functions
  void SetMockDeviceMotion(bool has_acceleration_x, double acceleration_x,
                           bool has_acceleration_y, double acceleration_y,
                           bool has_acceleration_z, double acceleration_z,
                           bool has_acceleration_including_gravity_x,
                           double acceleration_including_gravity_x,
                           bool has_acceleration_including_gravity_y,
                           double acceleration_including_gravity_y,
                           bool has_acceleration_including_gravity_z,
                           double acceleration_including_gravity_z,
                           bool has_rotation_rate_alpha,
                           double rotation_rate_alpha,
                           bool has_rotation_rate_beta,
                           double rotation_rate_beta,
                           bool has_rotation_rate_gamma,
                           double rotation_rate_gamma,
                           double interval);
  void SetMockDeviceOrientation(bool has_alpha, double alpha,
                                bool has_beta, double beta,
                                bool has_gamma, double gamma,
                                bool has_absolute, bool absolute);

  void SetMockScreenOrientation(const std::string& orientation);

  void DidChangeBatteryStatus(bool charging,
                              double chargingTime,
                              double dischargingTime,
                              double level);
  void ResetBatteryStatus();

  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();

  ///////////////////////////////////////////////////////////////////////////
  // Methods modifying WebPreferences.

  // Set the WebPreference that controls webkit's popup blocking.
  void SetPopupBlockingEnabled(bool block_popups);

  void SetJavaScriptCanAccessClipboard(bool can_access);
  void SetXSSAuditorEnabled(bool enabled);
  void SetAllowUniversalAccessFromFileURLs(bool allow);
  void SetAllowFileAccessFromFileURLs(bool allow);
  void OverridePreference(const std::string key, v8::Handle<v8::Value> value);

  // Modify accept_languages in RendererPreferences.
  void SetAcceptLanguages(const std::string& accept_languages);

  // Enable or disable plugins.
  void SetPluginsEnabled(bool enabled);

  ///////////////////////////////////////////////////////////////////////////
  // Methods that modify the state of TestRunner

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive text for each editing command. It takes no arguments, and
  // ignores any that may be present.
  void DumpEditingCallbacks();

  // This function sets a flag that tells the test_shell to dump pages as
  // plain text, rather than as a text representation of the renderer's state.
  // The pixel results will not be generated for this test.
  void DumpAsText();

  // This function sets a flag that tells the test_shell to dump pages as
  // the DOM contents, rather than as a text representation of the renderer's
  // state. The pixel results will not be generated for this test.
  void DumpAsMarkup();

  // This function sets a flag that tells the test_shell to dump pages as
  // plain text, rather than as a text representation of the renderer's state.
  // It will also generate a pixel dump for the test.
  void DumpAsTextWithPixelResults();

  // This function sets a flag that tells the test_shell to print out the
  // scroll offsets of the child frames. It ignores all.
  void DumpChildFrameScrollPositions();

  // This function sets a flag that tells the test_shell to recursively
  // dump all frames as plain text if the DumpAsText flag is set.
  // It takes no arguments, and ignores any that may be present.
  void DumpChildFramesAsText();

  // This function sets a flag that tells the test_shell to recursively
  // dump all frames as the DOM contents if the DumpAsMarkup flag is set.
  // It takes no arguments, and ignores any that may be present.
  void DumpChildFramesAsMarkup();

  // This function sets a flag that tells the test_shell to print out the
  // information about icon changes notifications from WebKit.
  void DumpIconChanges();

  // Deals with Web Audio WAV file data.
  void SetAudioData(const gin::ArrayBufferView& view);

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive text for each frame load callback. It takes no arguments, and
  // ignores any that may be present.
  void DumpFrameLoadCallbacks();

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive text for each PingLoader dispatch. It takes no arguments, and
  // ignores any that may be present.
  void DumpPingLoaderCallbacks();

  // This function sets a flag that tells the test_shell to print a line of
  // user gesture status text for some frame load callbacks. It takes no
  // arguments, and ignores any that may be present.
  void DumpUserGestureInFrameLoadCallbacks();

  void DumpTitleChanges();

  // This function sets a flag that tells the test_shell to dump all calls to
  // WebViewClient::createView().
  // It takes no arguments, and ignores any that may be present.
  void DumpCreateView();

  void SetCanOpenWindows();

  // This function sets a flag that tells the test_shell to dump a descriptive
  // line for each resource load callback. It takes no arguments, and ignores
  // any that may be present.
  void DumpResourceLoadCallbacks();

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive text for each element that requested a resource. It takes no
  // arguments, and ignores any that may be present.
  void DumpResourceRequestCallbacks();

  // This function sets a flag that tells the test_shell to dump the MIME type
  // for each resource that was loaded. It takes no arguments, and ignores any
  // that may be present.
  void DumpResourceResponseMIMETypes();

  // WebPermissionClient related.
  void SetImagesAllowed(bool allowed);
  void SetMediaAllowed(bool allowed);
  void SetScriptsAllowed(bool allowed);
  void SetStorageAllowed(bool allowed);
  void SetPluginsAllowed(bool allowed);
  void SetAllowDisplayOfInsecureContent(bool allowed);
  void SetAllowRunningOfInsecureContent(bool allowed);
  void DumpPermissionClientCallbacks();

  // This function sets a flag that tells the test_shell to dump all calls
  // to window.status().
  // It takes no arguments, and ignores any that may be present.
  void DumpWindowStatusChanges();

  // This function sets a flag that tells the test_shell to print a line of
  // descriptive text for the progress finished callback. It takes no
  // arguments, and ignores any that may be present.
  void DumpProgressFinishedCallback();

  // This function sets a flag that tells the test_shell to dump all
  // the lines of descriptive text about spellcheck execution.
  void DumpSpellCheckCallbacks();

  // This function sets a flag that tells the test_shell to print out a text
  // representation of the back/forward list. It ignores all arguments.
  void DumpBackForwardList();

  void DumpSelectionRect();

  // Causes layout to happen as if targetted to printed pages.
  void SetPrinting();

  // Clears the state from SetPrinting().
  void ClearPrinting();

  void SetShouldStayOnPageAfterHandlingBeforeUnload(bool value);

  // Causes WillSendRequest to clear certain headers.
  void SetWillSendRequestClearHeader(const std::string& header);

  // This function sets a flag that tells the test_shell to dump a descriptive
  // line for each resource load's priority and any time that priority
  // changes. It takes no arguments, and ignores any that may be present.
  void DumpResourceRequestPriorities();

  // Sets a flag to enable the mock theme.
  void SetUseMockTheme(bool use);

  // Sets a flag that causes the test to be marked as completed when the
  // WebFrameClient receives a loadURLExternally() call.
  void WaitUntilExternalURLLoad();

  ///////////////////////////////////////////////////////////////////////////
  // Methods interacting with the WebTestProxy

  ///////////////////////////////////////////////////////////////////////////
  // Methods forwarding to the WebTestDelegate

  // Shows DevTools window.
  void ShowWebInspector(const std::string& str,
                        const std::string& frontend_url);
  void CloseWebInspector();

  // Inspect chooser state
  bool IsChooserShown();

  // Allows layout tests to exec scripts at WebInspector side.
  void EvaluateInWebInspector(int call_id, const std::string& script);

  // Clears all databases.
  void ClearAllDatabases();
  // Sets the default quota for all origins
  void SetDatabaseQuota(int quota);

  // Changes the cookie policy from the default to allow all cookies.
  void SetAlwaysAcceptCookies(bool accept);

  // Gives focus to the window.
  void SetWindowIsKey(bool value);

  // Converts a URL starting with file:///tmp/ to the local mapping.
  std::string PathToLocalResource(const std::string& path);

  // Used to set the device scale factor.
  void SetBackingScaleFactor(double value, v8::Handle<v8::Function> callback);

  // Change the device color profile while running a layout test.
  void SetColorProfile(const std::string& name,
                       v8::Handle<v8::Function> callback);

  // Calls setlocale(LC_ALL, ...) for a specified locale.
  // Resets between tests.
  void SetPOSIXLocale(const std::string& locale);

  // MIDI function to control permission handling.
  void SetMIDIAccessorResult(bool result);
  void SetMIDISysexPermission(bool value);

  // Grants permission for desktop notifications to an origin
  void GrantWebNotificationPermission(const GURL& origin,
                                      bool permission_granted);

  // Clears all previously granted Web Notification permissions.
  void ClearWebNotificationPermissions();

  // Simulates a click on a desktop notification.
  bool SimulateWebNotificationClick(const std::string& value);

  // Speech recognition related functions.
  void AddMockSpeechRecognitionResult(const std::string& transcript,
                                      double confidence);
  void SetMockSpeechRecognitionError(const std::string& error,
                                     const std::string& message);
  bool WasMockSpeechRecognitionAborted();

  // WebPageOverlay related functions. Permits the adding and removing of only
  // one opaque overlay.
  void AddWebPageOverlay();
  void RemoveWebPageOverlay();

  void DisplayAsync();
  void DisplayAsyncThen(v8::Handle<v8::Function> callback);

  // Similar to DisplayAsyncThen(), but pass parameters of the captured
  // snapshot (width, height, snapshot) to the callback. The snapshot is in
  // uint8 RGBA format.
  void CapturePixelsAsyncThen(v8::Handle<v8::Function> callback);
  // Similar to CapturePixelsAsyncThen(). Copies to the clipboard the image
  // located at a particular point in the WebView (if there is such an image),
  // reads back its pixels, and provides the snapshot to the callback. If there
  // is no image at that point, calls the callback with (0, 0, empty_snapshot).
  void CopyImageAtAndCapturePixelsAsyncThen(
      int x, int y, const v8::Handle<v8::Function> callback);

  void SetMockPushClientSuccess(const std::string& endpoint,
                                const std::string& registration_id);
  void SetMockPushClientError(const std::string& message);

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  void CapturePixelsCallback(scoped_ptr<InvokeCallbackTask> task,
                             const SkBitmap& snapshot);

  void CheckResponseMimeType();
  void CompleteNotifyDone();

  void DidAcquirePointerLockInternal();
  void DidNotAcquirePointerLockInternal();
  void DidLosePointerLockInternal();

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading. From here, we can generate the dump for the
  // test.
  void LocationChangeDone();

  bool test_is_running_;

  // When reset is called, go through and close all but the main test shell
  // window. By default, set to true but toggled to false using
  // setCloseRemainingWindowsWhenComplete().
  bool close_remaining_windows_;

  // If true, don't dump output until notifyDone is called.
  bool wait_until_done_;

  // If true, ends the test when a URL is loaded externally via
  // WebFrameClient::loadURLExternally().
  bool wait_until_external_url_load_;

  // Causes navigation actions just printout the intended navigation instead
  // of taking you to the page. This is used for cases like mailto, where you
  // don't actually want to open the mail program.
  bool policy_delegate_enabled_;

  // Toggles the behavior of the policy delegate. If true, then navigations
  // will be allowed. Otherwise, they will be ignored (dropped).
  bool policy_delegate_is_permissive_;

  // If true, the policy delegate will signal layout test completion.
  bool policy_delegate_should_notify_done_;

  WorkQueue work_queue_;

  // Used by a number of layout tests in http/tests/security/dataURL.
  bool global_flag_;

  // Bound variable to return the name of this platform (chromium).
  std::string platform_name_;

  // Bound variable to store the last tooltip text
  std::string tooltip_text_;

  // Bound variable to disable notifyDone calls. This is used in GC leak
  // tests, where existing LayoutTests are loaded within an iframe. The GC
  // test harness will set this flag to ignore the notifyDone calls from the
  // target LayoutTest.
  bool disable_notify_done_;

  // Bound variable counting the number of top URLs visited.
  int web_history_item_count_;

  // Bound variable to set whether postMessages should be intercepted or not
  bool intercept_post_message_;

  // If true, the test_shell will write a descriptive line for each editing
  // command.
  bool dump_editting_callbacks_;

  // If true, the test_shell will generate pixel results in DumpAsText mode
  bool generate_pixel_results_;

  // If true, the test_shell will produce a plain text dump rather than a
  // text representation of the renderer.
  bool dump_as_text_;

  // If true and if dump_as_text_ is true, the test_shell will recursively
  // dump all frames as plain text.
  bool dump_child_frames_as_text_;

  // If true, the test_shell will produce a dump of the DOM rather than a text
  // representation of the renderer.
  bool dump_as_markup_;

  // If true and if dump_as_markup_ is true, the test_shell will recursively
  // produce a dump of the DOM rather than a text representation of the
  // renderer.
  bool dump_child_frames_as_markup_;

  // If true, the test_shell will print out the child frame scroll offsets as
  // well.
  bool dump_child_frame_scroll_positions_;

  // If true, the test_shell will print out the icon change notifications.
  bool dump_icon_changes_;

  // If true, the test_shell will output a base64 encoded WAVE file.
  bool dump_as_audio_;

  // If true, the test_shell will output a descriptive line for each frame
  // load callback.
  bool dump_frame_load_callbacks_;

  // If true, the test_shell will output a descriptive line for each
  // PingLoader dispatched.
  bool dump_ping_loader_callbacks_;

  // If true, the test_shell will output a line of the user gesture status
  // text for some frame load callbacks.
  bool dump_user_gesture_in_frame_load_callbacks_;

  // If true, output a message when the page title is changed.
  bool dump_title_changes_;

  // If true, output a descriptive line each time WebViewClient::createView
  // is invoked.
  bool dump_create_view_;

  // If true, new windows can be opened via javascript or by plugins. By
  // default, set to false and can be toggled to true using
  // setCanOpenWindows().
  bool can_open_windows_;

  // If true, the test_shell will output a descriptive line for each resource
  // load callback.
  bool dump_resource_load_callbacks_;

  // If true, the test_shell will output a descriptive line for each resource
  // request callback.
  bool dump_resource_request_callbacks_;

  // If true, the test_shell will output the MIME type for each resource that
  // was loaded.
  bool dump_resource_reqponse_mime_types_;

  // If true, the test_shell will dump all changes to window.status.
  bool dump_window_status_changes_;

  // If true, the test_shell will output a descriptive line for the progress
  // finished callback.
  bool dump_progress_finished_callback_;

  // If true, the test_shell will output descriptive test for spellcheck
  // execution.
  bool dump_spell_check_callbacks_;

  // If true, the test_shell will produce a dump of the back forward list as
  // well.
  bool dump_back_forward_list_;

  // If true, the test_shell will draw the bounds of the current selection rect
  // taking possible transforms of the selection rect into account.
  bool dump_selection_rect_;

  // If true, pixel dump will be produced as a series of 1px-tall, view-wide
  // individual paints over the height of the view.
  bool test_repaint_;

  // If true and test_repaint_ is true as well, pixel dump will be produced as
  // a series of 1px-wide, view-tall paints across the width of the view.
  bool sweep_horizontally_;

  // If true, layout is to target printed pages.
  bool is_printing_;

  // If false, MockWebMIDIAccessor fails on startSession() for testing.
  bool midi_accessor_result_;

  bool should_stay_on_page_after_handling_before_unload_;

  bool should_dump_resource_priorities_;

  bool has_custom_text_output_;
  std::string custom_text_output_;

  std::set<std::string> http_headers_to_clear_;

  // WAV audio data is stored here.
  std::vector<unsigned char> audio_data_;

  // Used for test timeouts.
  WebTaskList task_list_;

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebView* web_view_;
  TestPageOverlay* page_overlay_;
  WebTestProxyBase* proxy_;

  // This is non-0 IFF a load is in progress.
  blink::WebFrame* top_loading_frame_;

  // WebPermissionClient mock object.
  scoped_ptr<WebPermissions> web_permissions_;

  scoped_ptr<NotificationPresenter> notification_presenter_;

  bool pointer_locked_;
  enum {
    PointerLockWillSucceed,
    PointerLockWillRespondAsync,
    PointerLockWillFailSync,
  } pointer_lock_planned_result_;
  bool use_mock_theme_;

  base::WeakPtrFactory<TestRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_RUNNER_H_
