// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_TEST_RUNNER_H_
#define COMPONENTS_TEST_RUNNER_TEST_RUNNER_H_

#include <stdint.h>

#include <deque>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/test_runner/layout_test_runtime_flags.h"
#include "components/test_runner/test_runner_export.h"
#include "components/test_runner/web_test_runner.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "v8/include/v8.h"

class GURL;
class SkBitmap;

namespace blink {
class WebContentSettingsClient;
class WebFrame;
class WebLocalFrame;
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
class MockContentSettingsClient;
class MockCredentialManagerClient;
class MockScreenOrientationClient;
class MockWebSpeechRecognizer;
class MockWebUserMediaClient;
class SpellCheckClient;
class TestInterfaces;
class TestRunnerForSpecificView;
class WebTestDelegate;
class WebTestProxyBase;

// TestRunner class currently has dual purpose:
// 1. It implements |testRunner| javascript bindings for "global" / "ambient".
//    Examples:
//    - testRunner.dumpAsText (test flag affecting test behavior)
//    - testRunner.setAllowDisplayOfInsecureContent (test flag affecting product
//      behavior)
//    - testRunner.setTextSubpixelPositioning (directly interacts with product).
//    Note that "per-view" (non-"global") bindings are handled by
//    instances of TestRunnerForSpecificView class.
// 2. It manages global test state.  Example:
//    - Tracking topLoadingFrame that can finish the test when it loads.
//    - WorkQueue holding load requests from the TestInterfaces
//    - LayoutTestRuntimeFlags
class TestRunner : public WebTestRunner {
 public:
  explicit TestRunner(TestInterfaces*);
  virtual ~TestRunner();

  void Install(blink::WebLocalFrame* frame,
               base::WeakPtr<TestRunnerForSpecificView> view_test_runner);

  void SetDelegate(WebTestDelegate*);
  void SetMainView(blink::WebView*);

  void Reset();

  void SetTestIsRunning(bool);
  bool TestIsRunning() const { return test_is_running_; }

  bool UseMockTheme() const { return use_mock_theme_; }

  // WebTestRunner implementation.
  bool ShouldGeneratePixelResults() override;
  bool ShouldDumpAsAudio() const override;
  void GetAudioData(std::vector<unsigned char>* buffer_view) const override;
  bool IsRecursiveLayoutDumpRequested() override;
  std::string DumpLayout(blink::WebLocalFrame* frame) override;
  void DumpPixelsAsync(
      blink::WebView* web_view,
      const base::Callback<void(const SkBitmap&)>& callback) override;
  void ReplicateLayoutTestRuntimeFlagsChanges(
      const base::DictionaryValue& changed_values) override;
  bool HasCustomTextDump(std::string* custom_text_dump) const override;
  bool ShouldDumpBackForwardList() const override;
  blink::WebContentSettingsClient* GetWebContentSettings() const override;
  void InitializeWebViewWithMocks(blink::WebView* web_view) override;
  void SetFocus(blink::WebView* web_view, bool focus) override;

  // Methods used by WebViewTestClient and WebFrameTestClient.
  void OnAnimationScheduled(blink::WebView* view);
  void OnAnimationBegun(blink::WebView* view);
  std::string GetAcceptLanguages() const;
  bool shouldStayOnPageAfterHandlingBeforeUnload() const;
  MockScreenOrientationClient* getMockScreenOrientationClient();
  MockWebUserMediaClient* getMockWebUserMediaClient();
  MockWebSpeechRecognizer* getMockWebSpeechRecognizer();
  bool isPrinting() const;
  bool shouldDumpAsCustomText() const;
  std:: string customDumpText() const;
  void ShowDevTools(const std::string& settings,
                    const std::string& frontend_url);
  void ClearDevToolsLocalStorage();
  void setShouldDumpAsText(bool);
  void setShouldDumpAsMarkup(bool);
  void setCustomTextOutput(const std::string& text);
  void setShouldGeneratePixelResults(bool);
  void setShouldDumpFrameLoadCallbacks(bool);
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
  bool shouldDumpResourceResponseMIMETypes() const;
  bool shouldDumpStatusCallbacks() const;
  bool shouldDumpSpellCheckCallbacks() const;
  bool shouldWaitUntilExternalURLLoad() const;
  const std::set<std::string>* httpHeadersToClear() const;

  // To be called when |frame| starts loading - TestRunner will check if
  // there is currently no top-loading-frame being tracked and if so, then it
  // will return true and start tracking |frame| as the top-loading-frame.
  bool tryToSetTopLoadingFrame(blink::WebFrame* frame);

  // To be called when |frame| finishes loading - TestRunner will check if
  // |frame| is currently tracked as the top-loading-frame, and if yes, then it
  // will return true, stop top-loading-frame tracking, and potentially finish
  // the test (unless testRunner.waitUntilDone() was called and/or there are
  // pending load requests in WorkQueue).
  bool tryToClearTopLoadingFrame(blink::WebFrame*);

  blink::WebFrame* topLoadingFrame() const;
  void policyDelegateDone();
  bool policyDelegateEnabled() const;
  bool policyDelegateIsPermissive() const;
  bool policyDelegateShouldNotifyDone() const;
  bool shouldInterceptPostMessage() const;
  bool shouldDumpResourcePriorities() const;
  void setToolTipText(const blink::WebString&);
  void setDragImage(const blink::WebImage& drag_image);
  bool shouldDumpNavigationPolicy() const;

  bool midiAccessorResult();

  // Methods used by MockColorChooser:
  void DidOpenChooser();
  void DidCloseChooser();

  // A single item in the work queue.
  class WorkItem {
   public:
    virtual ~WorkItem() {}

    // Returns true if this started a load.
    virtual bool Run(WebTestDelegate*, blink::WebView*) = 0;
  };

 private:
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

   private:
    void ProcessWork();

    std::deque<WorkItem*> queue_;
    bool frozen_;
    TestRunner* controller_;

    base::WeakPtrFactory<WorkQueue> weak_factory_;
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

  // Allows layout tests to manage origins' whitelisting.
  void AddOriginAccessWhitelistEntry(const std::string& source_origin,
                                     const std::string& destination_protocol,
                                     const std::string& destination_host,
                                     bool allow_destination_subdomains);
  void RemoveOriginAccessWhitelistEntry(const std::string& source_origin,
                                        const std::string& destination_protocol,
                                        const std::string& destination_host,
                                        bool allow_destination_subdomains);

  // Add |source_code| as an injected stylesheet to the active document of the
  // window of the current V8 context.
  void InsertStyleSheet(const std::string& source_code);

  // Enables or disables subpixel positioning (i.e. fractional X positions for
  // glyphs) in text rendering on Linux. Since this method changes global
  // settings, tests that call it must use their own custom font family for
  // all text that they render. If not, an already-cached style will be used,
  // resulting in the changed setting being ignored.
  void SetTextSubpixelPositioning(bool value);

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
                                bool absolute);

  void SetMockScreenOrientation(const std::string& orientation);
  void DisableMockScreenOrientation();

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

  // Returns |true| if an animation has been scheduled in one or more WebViews
  // participating in the layout test.
  bool GetAnimationScheduled() const;

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

  // This function sets a flag to dump the drag image when the next drag&drop is
  // initiated. It is equivalent to DumpAsTextWithPixelResults but the pixel
  // results will be the drag image instead of a snapshot of the page.
  void DumpDragImage();

  // Sets a flag that tells the WebTestProxy to dump the default navigation
  // policy passed to the decidePolicyForNavigation callback.
  void DumpNavigationPolicy();

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

  // Converts a URL starting with file:///tmp/ to the local mapping.
  std::string PathToLocalResource(const std::string& path);

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

  // Simulates closing a Web Notification.
  void SimulateWebNotificationClose(const std::string& title, bool by_user);

  // Speech recognition related functions.
  void AddMockSpeechRecognitionResult(const std::string& transcript,
                                      double confidence);
  void SetMockSpeechRecognitionError(const std::string& error,
                                     const std::string& message);

  // Credential Manager mock functions
  // TODO(mkwst): Support FederatedCredential.
  void AddMockCredentialManagerResponse(const std::string& id,
                                        const std::string& name,
                                        const std::string& avatar,
                                        const std::string& password);
  void AddMockCredentialManagerError(const std::string& error);

  // Takes care of notifying the delegate after a change to layout test runtime
  // flags.
  void OnLayoutTestRuntimeFlagsChanged();

  ///////////////////////////////////////////////////////////////////////////
  // Internal helpers

  bool IsFramePartOfMainTestWindow(blink::WebFrame*) const;

  void CheckResponseMimeType();
  void CompleteNotifyDone();

  // In the Mac code, this is called to trigger the end of a test after the
  // page has finished loading. From here, we can generate the dump for the
  // test.
  void LocationChangeDone();

  bool test_is_running_;

  // When reset is called, go through and close all but the main test shell
  // window. By default, set to true but toggled to false using
  // setCloseRemainingWindowsWhenComplete().
  bool close_remaining_windows_;

  WorkQueue work_queue_;

  // Bound variable to return the name of this platform (chromium).
  std::string platform_name_;

  // Bound variable to store the last tooltip text
  std::string tooltip_text_;

  // Bound variable counting the number of top URLs visited.
  int web_history_item_count_;

  // Flags controlling what content gets dumped as a layout text result.
  LayoutTestRuntimeFlags layout_test_runtime_flags_;

  // If true, the test_shell will output a base64 encoded WAVE file.
  bool dump_as_audio_;

  // If true, output a descriptive line each time WebViewClient::createView
  // is invoked.
  bool dump_create_view_;

  // If true, new windows can be opened via javascript or by plugins. By
  // default, set to false and can be toggled to true using
  // setCanOpenWindows().
  bool can_open_windows_;

  // If true, the test_shell will dump all changes to window.status.
  bool dump_window_status_changes_;

  // If true, the test_shell will output descriptive test for spellcheck
  // execution.
  bool dump_spell_check_callbacks_;

  // If true, the test_shell will produce a dump of the back forward list as
  // well.
  bool dump_back_forward_list_;

  // If true, pixel dump will be produced as a series of 1px-tall, view-wide
  // individual paints over the height of the view.
  bool test_repaint_;

  // If true and test_repaint_ is true as well, pixel dump will be produced as
  // a series of 1px-wide, view-tall paints across the width of the view.
  bool sweep_horizontally_;

  // If false, MockWebMIDIAccessor fails on startSession() for testing.
  bool midi_accessor_result_;

  bool has_custom_text_output_;
  std::string custom_text_output_;

  std::set<std::string> http_headers_to_clear_;

  // WAV audio data is stored here.
  std::vector<unsigned char> audio_data_;

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebView* main_view_;

  // This is non-0 IFF a load is in progress.
  blink::WebFrame* top_loading_frame_;

  // WebContentSettingsClient mock object.
  std::unique_ptr<MockContentSettingsClient> mock_content_settings_client_;

  bool use_mock_theme_;

  std::unique_ptr<MockCredentialManagerClient> credential_manager_client_;
  std::unique_ptr<MockScreenOrientationClient> mock_screen_orientation_client_;
  std::unique_ptr<MockWebSpeechRecognizer> speech_recognizer_;
  std::unique_ptr<MockWebUserMediaClient> user_media_client_;
  std::unique_ptr<SpellCheckClient> spellcheck_;

  // Number of currently active color choosers.
  int chooser_count_;

  // Captured drag image.
  blink::WebImage drag_image_;

  // View that was focused by a previous call to TestRunner::SetFocus method.
  // Note - this can be a dangling pointer to an already destroyed WebView (this
  // is ok, because this is taken care of in WebTestDelegate::SetFocus).
  blink::WebView* previously_focused_view_;

  std::set<blink::WebView*> views_with_scheduled_animations_;

  base::WeakPtrFactory<TestRunner> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRunner);
};

// TestRunnerForSpecificView implements part of |testRunner| javascript bindings
// that work with a view where the javascript call originated from.  Examples:
// - testRunner.capturePixelsAsyncThen
// - testRunner.setPageVisibility
// Note that "global" bindings are handled by TestRunner class.
// TODO(lukasza): Move this class to a separate compilation unit.
class TestRunnerForSpecificView {
 public:
  explicit TestRunnerForSpecificView(WebTestProxyBase* web_test_proxy_base);
  ~TestRunnerForSpecificView();

  // Installs view-specific bindings (handled by |this|) and *also* global
  // TestRunner bindings (both kinds of bindings are exposed via a single
  // |testRunner| object in javascript).
  void Install(blink::WebLocalFrame* frame);

  void Reset();

  // Pointer lock methods used by WebViewTestClient.
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool isPointerLocked();

 private:
  friend class TestRunnerBindings;

  // Helpers for working with base and V8 callbacks.
  void PostTask(const base::Closure& callback);
  void PostDelayedTask(long long delay, const base::Closure& callback);
  void PostV8Callback(const v8::Local<v8::Function>& callback);
  void PostV8CallbackWithArgs(v8::UniquePersistent<v8::Function> callback,
                              int argc,
                              v8::Local<v8::Value> argv[]);
  void InvokeV8Callback(const v8::UniquePersistent<v8::Function>& callback);
  void InvokeV8CallbackWithArgs(
      const v8::UniquePersistent<v8::Function>& callback,
      const std::vector<v8::UniquePersistent<v8::Value>>& args);
  base::Closure CreateClosureThatPostsV8Callback(
      const v8::Local<v8::Function>& callback);

  void LayoutAndPaintAsync();
  void LayoutAndPaintAsyncThen(v8::Local<v8::Function> callback);

  // Similar to LayoutAndPaintAsyncThen(), but pass parameters of the captured
  // snapshot (width, height, snapshot) to the callback. The snapshot is in
  // uint8_t RGBA format.
  void CapturePixelsAsyncThen(v8::Local<v8::Function> callback);
  void CapturePixelsCallback(v8::UniquePersistent<v8::Function> callback,
                             const SkBitmap& snapshot);

  // Similar to CapturePixelsAsyncThen(). Copies to the clipboard the image
  // located at a particular point in the WebView (if there is such an image),
  // reads back its pixels, and provides the snapshot to the callback. If there
  // is no image at that point, calls the callback with (0, 0, empty_snapshot).
  void CopyImageAtAndCapturePixelsAsyncThen(
      int x,
      int y,
      const v8::Local<v8::Function> callback);

  void GetManifestThen(v8::Local<v8::Function> callback);
  void GetManifestCallback(v8::UniquePersistent<v8::Function> callback,
                           const blink::WebURLResponse& response,
                           const std::string& data);

  // Calls |callback| with a DOMString[] representing the events recorded since
  // the last call to this function.
  void GetBluetoothManualChooserEvents(v8::Local<v8::Function> callback);
  void GetBluetoothManualChooserEventsCallback(
      v8::UniquePersistent<v8::Function> callback,
      const std::vector<std::string>& events);

  // Change the bluetooth test data while running a layout test.
  void SetBluetoothFakeAdapter(const std::string& adapter_name,
                               v8::Local<v8::Function> callback);

  // If |enable| is true, makes the Bluetooth chooser record its input and wait
  // for instructions from the test program on how to proceed. Otherwise falls
  // back to the browser's default chooser.
  void SetBluetoothManualChooser(bool enable);

  // Calls the BluetoothChooser::EventHandler with the arguments here. Valid
  // event strings are:
  //  * "cancel" - simulates the user canceling the chooser.
  //  * "select" - simulates the user selecting a device whose device ID is in
  //               |argument|.
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument);

  // Used to set the device scale factor.
  void SetBackingScaleFactor(double value, v8::Local<v8::Function> callback);

  // Enable zoom-for-dsf option.
  // TODO(oshima): Remove this once all platforms migrated.
  void EnableUseZoomForDSF(v8::Local<v8::Function> callback);

  // Change the device color profile while running a layout test.
  void SetColorProfile(const std::string& name,
                       v8::Local<v8::Function> callback);

  // Causes the beforeinstallprompt event to be sent to the renderer.
  void DispatchBeforeInstallPromptEvent(
      int request_id,
      const std::vector<std::string>& event_platforms,
      v8::Local<v8::Function> callback);
  void DispatchBeforeInstallPromptCallback(
      v8::UniquePersistent<v8::Function> callback,
      bool canceled);

  // Immediately run all pending idle tasks, including all pending
  // requestIdleCallback calls.  Invoke the callback when all
  // idle tasks are complete.
  void RunIdleTasks(v8::Local<v8::Function> callback);

  // Method that controls whether pressing Tab key cycles through page elements
  // or inserts a '\t' char in text area
  void SetTabKeyCyclesThroughElements(bool tab_key_cycles_through_elements);

  // Executes an internal command (superset of document.execCommand() commands).
  void ExecCommand(gin::Arguments* args);

  // Checks if an internal command is currently available.
  bool IsCommandEnabled(const std::string& command);

  // Returns true if the current page box has custom page size style for
  // printing.
  bool HasCustomPageSizeStyle(int page_index);

  // Forces the selection colors for testing under Linux.
  void ForceRedSelectionColors();

  // Switch the visibility of the page.
  void SetPageVisibility(const std::string& new_visibility);

  // Changes the direction of the focused element.
  void SetTextDirection(const std::string& direction_name);

  // Dump current PageImportanceSignals for the page.
  void DumpPageImportanceSignals();

  // WebPageOverlay related functions. Permits the adding and removing of only
  // one opaque overlay.
  void AddWebPageOverlay();
  void RemoveWebPageOverlay();

  // Sets a flag causing the next call to WebGLRenderingContext::create to fail.
  void ForceNextWebGLContextCreationToFail();

  // Sets a flag causing the next call to DrawingBuffer::create to fail.
  void ForceNextDrawingBufferCreationToFail();

  // Gives focus to the view associated with TestRunnerForSpecificView.
  void SetWindowIsKey(bool value);

  // Pointer lock handling.
  void DidAcquirePointerLock();
  void DidNotAcquirePointerLock();
  void DidLosePointerLock();
  void SetPointerLockWillFailSynchronously();
  void SetPointerLockWillRespondAsynchronously();
  void DidAcquirePointerLockInternal();
  void DidNotAcquirePointerLockInternal();
  void DidLosePointerLockInternal();
  bool pointer_locked_;
  enum {
    PointerLockWillSucceed,
    PointerLockWillRespondAsync,
    PointerLockWillFailSync,
  } pointer_lock_planned_result_;

  bool CallShouldCloseOnWebView();
  void SetDomainRelaxationForbiddenForURLScheme(bool forbidden,
                                                const std::string& scheme);
  v8::Local<v8::Value> EvaluateScriptInIsolatedWorldAndReturnValue(
      int world_id,
      const std::string& script);
  void EvaluateScriptInIsolatedWorld(int world_id, const std::string& script);
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      v8::Local<v8::Value> origin);
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const std::string& policy);
  bool FindString(const std::string& search_text,
                  const std::vector<std::string>& options_array);
  std::string SelectionAsMarkup();
  void SetViewSourceForFrame(const std::string& name, bool enabled);

  // Helpers for accessing pointers exposed by |web_test_proxy_base_|.
  blink::WebView* web_view();
  WebTestDelegate* delegate();
  WebTestProxyBase* web_test_proxy_base_;

  base::WeakPtrFactory<TestRunnerForSpecificView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestRunnerForSpecificView);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_TEST_RUNNER_H_
