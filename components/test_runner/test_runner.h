// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_RUNNER_H_
#define COMPONENTS_TEST_RUNNER_TEST_RUNNER_H_

#include <stdint.h>

#include <deque>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/test_runner/test_runner_export.h"
#include "components/test_runner/web_task.h"
#include "components/test_runner/web_test_runner.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

namespace blink {
class WebContentSettingsClient;
class WebFrame;
class WebMediaStream;
class WebString;
class WebView;
class WebURLResponse;
}

namespace gin {
class ArrayBufferView;
class Arguments;
}

namespace test_runner {

class InvokeCallbackTask;
class TestInterfaces;
class WebContentSettings;
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
  bool ShouldGeneratePixelResults() override;
  bool ShouldStayOnPageAfterHandlingBeforeUnload() const override;
  bool ShouldDumpAsAudio() const override;
  void GetAudioData(std::vector<unsigned char>* buffer_view) const override;
  LayoutDumpFlags GetLayoutDumpFlags() override;
  bool HasCustomTextDump(std::string* custom_text_dump) const override;
  bool ShouldDumpBackForwardList() const override;
  blink::WebContentSettingsClient* GetWebContentSettings() const override;

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
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void ClearDevToolsLocalStorage();
  void setShouldDumpAsText(bool);
  void setShouldDumpAsMarkup(bool);
  void setCustomTextOutput(const std::string& text);
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
  bool shouldDumpSpellCheckCallbacks() const;
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
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool isPointerLocked();
  void setToolTipText(const blink::WebString&);
  bool shouldDumpDragImage();
  bool shouldDumpNavigationPolicy() const;

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

      void RunIfValid() override;
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
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int world_id, const std::string& script);
  void EvaluateScriptInIsolatedWorld(int world_id, const std::string& script);
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      v8::Local<v8::Value> origin);
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

  // Add |source_code| as an injected stylesheet to the active document of the
  // window of the current V8 context.
  void InsertStyleSheet(const std::string& source_code);

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
  void OverridePreference(const std::string& key, v8::Local<v8::Value> value);

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

  // WebContentSettingsClient related.
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

  // This function sets a flag which tells the WebTestProxy to dump the drag
  // image when the next drag-and-drop is initiated. It is equivalent to
  // DumpAsTextWithPixelResults but the pixel results will be the drag image
  // instead of a snapshot of the page.
  void DumpDragImage();

  // Sets a flag that tells the WebTestProxy to dump the default navigation
  // policy passed to the decidePolicyForNavigation callback.
  void DumpNavigationPolicy();

  // Dump current PageImportanceSignals for the page.
  void DumpPageImportanceSignals();

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
  // Allows layout tests to evaluate scripts in InspectorOverlay page.
  // Script may have an output represented as a string, return values of other
  // types would be ignored.
  std::string EvaluateInWebInspectorOverlay(const std::string& script);

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
  void SetBackingScaleFactor(double value, v8::Local<v8::Function> callback);

  // Enable zoom-for-dsf option.
  // TODO(oshima): Remove this once all platforms migrated.
  void EnableUseZoomForDSF(v8::Local<v8::Function> callback);

  // Change the device color profile while running a layout test.
  void SetColorProfile(const std::string& name,
                       v8::Local<v8::Function> callback);

  // Change the bluetooth test data while running a layout test and resets the
  // chooser to accept the first device.
  void SetBluetoothMockDataSet(const std::string& name);

  // Makes the Bluetooth chooser record its input and wait for instructions from
  // the test program on how to proceed.
  void SetBluetoothManualChooser();

  // Calls |callback| with a DOMString[] representing the events recorded since
  // the last call to this function.
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);

  // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
  // event strings are:
  //  * "cancel" - simulates the user canceling the chooser.
  //  * "select" - simulates the user selecting a device whose device ID is in
  //               |argument|.
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);

  // Enables mock geofencing service while running a layout test.
  // |service_available| indicates if the mock service should mock geofencing
  // being available or not.
  void SetGeofencingMockProvider(bool service_available);

  // Disables mock geofencing service while running a layout test.
  void ClearGeofencingMockProvider();

  // Set the mock geofencing position while running a layout test.
  void SetGeofencingMockPosition(double latitude, double longitude);

  // Sets the permission's |name| to |value| for a given {origin, embedder}
  // tuple.
  void SetPermission(const std::string& name,
                     const std::string& value,
                     const GURL& origin,
                     const GURL& embedding_origin);

  // Causes the beforeinstallprompt event to be sent to the renderer.
  void DispatchBeforeInstallPromptEvent(
      int request_id,
      const std::vector<std::string>& event_platforms,
      v8::Local<v8::Function> callback);

  // Resolve the beforeinstallprompt event with the matching request id.
  void ResolveBeforeInstallPromptPromise(int request_id,
                                         const std::string& platform);

  // Calls setlocale(LC_ALL, ...) for a specified locale.
  // Resets between tests.
  void SetPOSIXLocale(const std::string& locale);

  // MIDI function to control permission handling.
  void SetMIDIAccessorResult(bool result);

  // Simulates a click on a Web Notification.
  void SimulateWebNotificationClick(const std::string& title, int action_index);

  // Speech recognition related functions.
  void AddMockSpeechRecognitionResult(const std::string& transcript,
                                      double confidence);
  void SetMockSpeechRecognitionError(const std::string& error,
                                     const std::string& message);
  bool WasMockSpeechRecognitionAborted();

  // Credential Manager mock functions
  // TODO(mkwst): Support FederatedCredential.
  void AddMockCredentialManagerResponse(const std::string& id,
                                        const std::string& name,
                                        const std::string& avatar,
                                        const std::string& password);

  // WebPageOverlay related functions. Permits the adding and removing of only
  // one opaque overlay.
  void AddWebPageOverlay();
  void RemoveWebPageOverlay();

  void LayoutAndPaintAsync();
  void LayoutAndPaintAsyncThen(v8::Local<v8::Function> callback);

  // Similar to LayoutAndPaintAsyncThen(), but pass parameters of the captured
  // snapshot (width, height, snapshot) to the callback. The snapshot is in
  // uint8_t RGBA format.
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);
  // Similar to CapturePixelsAsyncThen(). Copies to the clipboard the image
  // located at a particular point in the WebView (if there is such an image),
  // reads back its pixels, and provides the snapshot to the callback. If there
  // is no image at that point, calls the callback with (0, 0, empty_snapshot).
  void CopyImageAtAndCapturePixelsAsyncThen(
      int x, int y, const v8::Local<v8::Function> callback);

  void GetManifestThen(v8::Local<v8::Function> callback);

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  void GetManifestCallback(scoped_ptr<InvokeCallbackTask> task,
                           const blink::WebURLResponse& response,
                           const std::string& data);
  void CapturePixelsCallback(scoped_ptr<InvokeCallbackTask> task,
                             const SkBitmap& snapshot);
  void DispatchBeforeInstallPromptCallback(scoped_ptr<InvokeCallbackTask> task,
                                           bool canceled);
  void GetBluetoothManualChooserEventsCallback(
      scoped_ptr<InvokeCallbackTask> task,
      const std::vector<std::string>& events);

  void CheckResponseMimeType();
  void CompleteNotifyDone();

  void DidAcquirePointerLockInternal();
  void DidNotAcquirePointerLockInternal();
  void DidLosePointerLockInternal();

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading. From here, we can generate the dump for the
  // test.
  void LocationChangeDone();

  // Sets a flag causing the next call to WebGLRenderingContext::create to fail.
  void ForceNextWebGLContextCreationToFail();

  // Sets a flag causing the next call to DrawingBuffer::create to fail.
  void ForceNextDrawingBufferCreationToFail();

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
  bool dump_resource_response_mime_types_;

  // If true, the test_shell will dump all changes to window.status.
  bool dump_window_status_changes_;

  // If true, the test_shell will output descriptive test for spellcheck
  // execution.
  bool dump_spell_check_callbacks_;

  // If true, the test_shell will produce a dump of the back forward list as
  // well.
  bool dump_back_forward_list_;

  // If true, the test_shell will draw the bounds of the current selection rect
  // taking possible transforms of the selection rect into account.
  bool dump_selection_rect_;

  // If true, the test_shell will dump the drag image as pixel results.
  bool dump_drag_image_;

  // If true, content_shell will dump the default navigation policy passed to
  // WebFrameClient::decidePolicyForNavigation.
  bool dump_navigation_policy_;

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
  WebTestProxyBase* proxy_;

  // This is non-0 IFF a load is in progress.
  blink::WebFrame* top_loading_frame_;

  // WebContentSettingsClient mock object.
  scoped_ptr<WebContentSettings> web_content_settings_;

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

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_TEST_RUNNER_H_
