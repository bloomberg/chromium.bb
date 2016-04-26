// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
#define IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "ios/web/navigation/navigation_manager_delegate.h"
#include "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/net/request_tracker_impl.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

@protocol CRWRequestTrackerDelegate;
@class CRWWebController;
@protocol CRWWebViewProxy;
@class NSURLRequest;
@class NSURLResponse;

namespace net {
class HttpResponseHeaders;
}

namespace web {

class BrowserState;
struct Credential;
struct FaviconURL;
struct LoadCommittedDetails;
class NavigationManager;
class WebInterstitialImpl;
class WebStateDelegate;
class WebStateFacadeDelegate;
class WebStatePolicyDecider;
class WebUIIOS;

// Implementation of WebState.
// Generally mirrors //content's WebContents implementation.
// General notes on expected WebStateImpl ownership patterns:
//  - Outside of tests, WebStateImpls are created
//      (a) By @Tab, when creating a new Tab.
//      (b) By @SessionWindow, when decoding a saved session.
//      (c) By the Copy() method, below, used when marshalling a session
//          in preparation for saving.
//  - WebControllers are the eventual long-term owners of WebStateImpls.
//  - SessionWindows are transient owners, passing ownership into WebControllers
//    during session restore, and discarding owned copies of WebStateImpls after
//    writing them out for session saves.
class WebStateImpl : public WebState, public NavigationManagerDelegate {
 public:
  WebStateImpl(BrowserState* browser_state);
  ~WebStateImpl() override;

  // Gets/Sets the CRWWebController that backs this object.
  CRWWebController* GetWebController();
  void SetWebController(CRWWebController* web_controller);

  // Gets or sets the delegate used to communicate with the web contents facade.
  WebStateFacadeDelegate* GetFacadeDelegate() const;
  void SetFacadeDelegate(WebStateFacadeDelegate* facade_delegate);

  // Returns a WebStateImpl that doesn't have a browser context, web
  // controller, or facade set, but which otherwise has the same state variables
  // as the calling object (including copies of the NavigationManager and its
  // attendant CRWSessionController).
  // TODO(crbug.com/546377): Clean up this method.
  WebStateImpl* CopyForSessionWindow();

  // Notifies the observers that a provisional navigation has started.
  void OnProvisionalNavigationStarted(const GURL& url);

  // Called when a navigation is committed.
  void OnNavigationCommitted(const GURL& url);

  // Notifies the observers that the URL hash of the current page changed.
  void OnUrlHashChanged();

  // Notifies the observers that the history state of the current page changed.
  void OnHistoryStateChanged();

  // Called when a script command is received.
  // Returns true if the command was handled.
  bool OnScriptCommandReceived(const std::string& command,
                               const base::DictionaryValue& value,
                               const GURL& url,
                               bool user_is_interacting);

  void SetIsLoading(bool is_loading);

  // Called when a page is loaded. Must be called only once per page.
  void OnPageLoaded(const GURL& url, bool load_success);

  // Called on form submission.
  void OnDocumentSubmitted(const std::string& form_name, bool user_initiated);

  // Called when form activity is registered.
  void OnFormActivityRegistered(const std::string& form_name,
                                const std::string& field_name,
                                const std::string& type,
                                const std::string& value,
                                int key_code,
                                bool input_missing);

  // Called when new FaviconURL candidates are received.
  void OnFaviconUrlUpdated(const std::vector<FaviconURL>& candidates);

  // Called when the page requests a credential.
  void OnCredentialsRequested(int request_id,
                              const GURL& source_url,
                              bool unmediated,
                              const std::vector<std::string>& federations,
                              bool user_interaction);

  // Called when the page sends a notification that the user signed in with
  // |credential|.
  void OnSignedIn(int request_id,
                  const GURL& source_url,
                  const web::Credential& credential);

  // Called when the page sends a notification that the user signed in.
  void OnSignedIn(int request_id, const GURL& source_url);

  // Called when the page sends a notification that the user was signed out.
  void OnSignedOut(int request_id, const GURL& source_url);

  // Called when the page sends a notification that the user failed to sign in
  // with |credential|.
  void OnSignInFailed(int request_id,
                      const GURL& source_url,
                      const web::Credential& credential);

  // Called when the page sends a notification that the user failed to sign in.
  void OnSignInFailed(int request_id, const GURL& source_url);

  // Returns the NavigationManager for this WebState.
  const NavigationManagerImpl& GetNavigationManagerImpl() const;
  NavigationManagerImpl& GetNavigationManagerImpl();

  // Creates a WebUI page for the given url, owned by this object.
  void CreateWebUI(const GURL& url);
  // Clears any current WebUI. Should be called when the page changes.
  // TODO(stuartmorgan): Remove once more logic is moved from WebController
  // into this class.
  void ClearWebUI();
  // Returns true if there is a WebUI active.
  bool HasWebUI();
  // Processes a message from a WebUI displayed at the given URL.
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args);
  // Invokes page load for WebUI URL with HTML. URL must have an application
  // specific scheme.
  virtual void LoadWebUIHtml(const base::string16& html, const GURL& url);

  // Gets the HTTP response headers associated with the current page.
  // NOTE: For a WKWebView-based WebState, these headers are generated via
  // net::CreateHeadersFromNSHTTPURLResponse(); see comments in
  // http_response_headers_util.h for limitations.
  net::HttpResponseHeaders* GetHttpResponseHeaders() const;

  // Called when HTTP response headers are received.
  // |resource_url| is the URL associated with the headers.
  // This function has no visible effects until UpdateHttpResponseHeaders() is
  // called.
  void OnHttpResponseHeadersReceived(net::HttpResponseHeaders* response_headers,
                                     const GURL& resource_url);

  // Explicitly sets the MIME type, overwriting any MIME type that was set by
  // headers. Note that this should be called after OnNavigationCommitted, as
  // that is the point where MIME type is set from HTTP headers.
  void SetContentsMimeType(const std::string& mime_type);

  // Executes a JavaScript string on the page asynchronously.
  // TODO(shreyasv): Rename this to ExecuteJavaScript for consistency with
  // upstream API.
  virtual void ExecuteJavaScriptAsync(const base::string16& script);

  // Returns whether the navigation corresponding to |request| should be allowed
  // to continue by asking its policy deciders. Defaults to true.
  bool ShouldAllowRequest(NSURLRequest* request);
  // Returns whether the navigation corresponding to |response| should be
  // allowed to continue by asking its policy deciders. Defaults to true.
  bool ShouldAllowResponse(NSURLResponse* response);

  // Request tracker management. For now, this exposes the RequestTracker for
  // embedders to use.
  // TODO(stuartmorgan): RequestTracker should become an internal detail of this
  // class.

  // Create a new tracker using |delegate| as its delegate.
  void InitializeRequestTracker(id<CRWRequestTrackerDelegate> delegate);

  // Close the request tracker and delete it.
  void CloseRequestTracker();

  // Returns the tracker for this WebStateImpl.
  RequestTrackerImpl* GetRequestTracker();

  // Lazily creates (if necessary) and returns |request_group_id_|.
  // IMPORTANT: This should not be used for anything other than associating this
  // instance to network requests.
  // This function is only intended to be used in web/.
  // TODO(stuartmorgan): Move this method in an implementation file in web/.
  NSString* GetRequestGroupID();

  // WebState:
  WebStateDelegate* GetDelegate() override;
  void SetDelegate(WebStateDelegate* delegate) override;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool enabled) override;
  UIView* GetView() override;
  BrowserState* GetBrowserState() const override;
  void OpenURL(const WebState::OpenURLParams& params) override;
  NavigationManager* GetNavigationManager() override;
  CRWJSInjectionReceiver* GetJSInjectionReceiver() const override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         const JavaScriptResultCallback& callback) override;
  const std::string& GetContentLanguageHeader() const override;
  const std::string& GetContentsMimeType() const override;
  bool ContentIsHTML() const override;
  const base::string16& GetTitle() const override;
  bool IsLoading() const override;
  double GetLoadingProgress() const override;
  bool IsBeingDestroyed() const override;
  const GURL& GetVisibleURL() const override;
  const GURL& GetLastCommittedURL() const override;
  GURL GetCurrentURL(URLVerificationTrustLevel* trust_level) const override;
  void ShowTransientContentView(CRWContentView* content_view) override;
  bool IsShowingWebInterstitial() const override;
  WebInterstitial* GetWebInterstitial() const override;
  int GetCertGroupId() const override;
  void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                const std::string& command_prefix) override;
  void RemoveScriptCommandCallback(const std::string& command_prefix) override;
  id<CRWWebViewProxy> GetWebViewProxy() const override;
  int DownloadImage(const GURL& url,
                    bool is_favicon,
                    uint32_t max_bitmap_size,
                    bool bypass_cache,
                    const ImageDownloadCallback& callback) override;
  base::WeakPtr<WebState> AsWeakPtr() override;

  // Adds |interstitial|'s view to the web controller's content view.
  void ShowWebInterstitial(WebInterstitialImpl* interstitial);

  // Called to dismiss the currently-displayed transient content view.
  void ClearTransientContentView();

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress(double progress);

  // NavigationManagerDelegate:
  void NavigateToPendingEntry() override;
  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void OnNavigationItemsPruned(size_t pruned_item_count) override;
  void OnNavigationItemChanged() override;
  void OnNavigationItemCommitted(
      const LoadCommittedDetails& load_details) override;
  WebState* GetWebState() override;

 protected:
  void AddObserver(WebStateObserver* observer) override;
  void RemoveObserver(WebStateObserver* observer) override;
  void AddPolicyDecider(WebStatePolicyDecider* decider) override;
  void RemovePolicyDecider(WebStatePolicyDecider* decider) override;

 private:
  // Creates a WebUIIOS object for |url| that is owned by the caller. Returns
  // nullptr if |url| does not correspond to a WebUI page.
  WebUIIOS* CreateWebUIIOS(const GURL& url);

  // Updates the HTTP response headers for the main page using the headers
  // passed to the OnHttpResponseHeadersReceived() function below.
  // GetHttpResponseHeaders() can be used to get the headers.
  void UpdateHttpResponseHeaders(const GURL& url);

  // Returns true if |web_controller_| has been set.
  bool Configured() const;

  // Delegate, not owned by this object.
  WebStateDelegate* delegate_;

  // Stores whether the web state is currently loading a page.
  bool is_loading_;

  // Stores whether the web state is currently being destroyed.
  bool is_being_destroyed_;

  // The delegate used to pass state to the web contents facade.
  WebStateFacadeDelegate* facade_delegate_;

  // The CRWWebController that backs this object.
  base::scoped_nsobject<CRWWebController> web_controller_;

  NavigationManagerImpl navigation_manager_;

  // |web::WebUIIOS| object for the current page if it is a WebUI page that
  // uses the web-based WebUI framework, or nullptr otherwise.
  std::unique_ptr<web::WebUIIOS> web_ui_;

  // A list of observers notified when page state changes. Weak references.
  base::ObserverList<WebStateObserver, true> observers_;

  // All the WebStatePolicyDeciders asked for navigation decision. Weak
  // references.
  // WebStatePolicyDeciders are semantically different from observers (they
  // modify the behavior of the WebState) but are used like observers in the
  // code, hence the ObserverList.
  base::ObserverList<WebStatePolicyDecider, true> policy_deciders_;

  // Map of all the HTTP response headers received, for each URL.
  // This map is cleared after each page load, and only the headers of the main
  // page are used.
  std::map<GURL, scoped_refptr<net::HttpResponseHeaders> >
      response_headers_map_;
  scoped_refptr<net::HttpResponseHeaders> http_response_headers_;
  std::string mime_type_;
  std::string content_language_header_;

  // Weak pointer to the interstitial page being displayed, if any.
  WebInterstitialImpl* interstitial_;

  // Returned by reference.
  base::string16 empty_string16_;

  // Request tracker associted with this object.
  scoped_refptr<RequestTrackerImpl> request_tracker_;

  // A number identifying this object. This number is injected into the user
  // agent to allow the network layer to know which web view requests originated
  // from.
  base::scoped_nsobject<NSString> request_group_id_;

  // Callbacks associated to command prefixes.
  std::map<std::string, ScriptCommandCallback> script_command_callbacks_;

  // Member variables should appear before the WeakPtrFactory<> to ensure that
  // any WeakPtrs to WebStateImpl are invalidated before its member variable's
  // destructors are executed, rendering them invalid.
  base::WeakPtrFactory<WebState> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebStateImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
