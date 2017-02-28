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
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/java_script_dialog_callback.h"
#include "ios/web/public/java_script_dialog_type.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_delegate.h"
#include "url/gurl.h"

@class CRWSessionStorage;
@class CRWWebController;
@protocol CRWWebViewProxy;
@class NSURLRequest;
@class NSURLResponse;

namespace net {
class HttpResponseHeaders;
}

namespace web {

class BrowserState;
struct ContextMenuParams;
struct Credential;
struct FaviconURL;
struct LoadCommittedDetails;
class NavigationManager;
class WebInterstitialImpl;
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
  // Constructor for WebStateImpls created for new sessions.
  WebStateImpl(BrowserState* browser_state);
  // Constructor for WebStatesImpls created for deserialized sessions
  WebStateImpl(BrowserState* browser_state, CRWSessionStorage* session_storage);
  ~WebStateImpl() override;

  // Gets/Sets the CRWWebController that backs this object.
  CRWWebController* GetWebController();
  void SetWebController(CRWWebController* web_controller);

  // Gets or sets the delegate used to communicate with the web contents facade.
  WebStateFacadeDelegate* GetFacadeDelegate() const;
  void SetFacadeDelegate(WebStateFacadeDelegate* facade_delegate);

  // Notifies the observers that a provisional navigation has started.
  void OnProvisionalNavigationStarted(const GURL& url);

  // Called when a navigation is committed.
  void OnNavigationCommitted(const GURL& url);

  // Notifies the observers that same page navigation did finish.
  void OnSamePageNavigation(const GURL& url);

  // Notifies the observers that navigation to error page did finish.
  void OnErrorPageNavigation(const GURL& url);

  // Called when page title was changed.
  void OnTitleChanged();

  // Notifies the observers that the render process was terminated.
  void OnRenderProcessGone();

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

  // Returns whether the navigation corresponding to |request| should be allowed
  // to continue by asking its policy deciders. Defaults to true.
  bool ShouldAllowRequest(NSURLRequest* request);
  // Returns whether the navigation corresponding to |response| should be
  // allowed to continue by asking its policy deciders. Defaults to true.
  bool ShouldAllowResponse(NSURLResponse* response);

  // WebState:
  WebStateDelegate* GetDelegate() override;
  void SetDelegate(WebStateDelegate* delegate) override;
  bool IsWebUsageEnabled() const override;
  void SetWebUsageEnabled(bool enabled) override;
  bool ShouldSuppressDialogs() const override;
  void SetShouldSuppressDialogs(bool should_suppress) override;
  UIView* GetView() override;
  BrowserState* GetBrowserState() const override;
  void OpenURL(const WebState::OpenURLParams& params) override;
  void Stop() override;
  const NavigationManager* GetNavigationManager() const override;
  NavigationManager* GetNavigationManager() override;
  CRWSessionStorage* BuildSessionStorage() override;
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
  void OnPasswordInputShownOnHttp() override;
  void OnCreditCardInputShownOnHttp() override;
  void AddScriptCommandCallback(const ScriptCommandCallback& callback,
                                const std::string& command_prefix) override;
  void RemoveScriptCommandCallback(const std::string& command_prefix) override;
  id<CRWWebViewProxy> GetWebViewProxy() const override;
  service_manager::InterfaceRegistry* GetMojoInterfaceRegistry() override;
  base::WeakPtr<WebState> AsWeakPtr() override;

  // Adds |interstitial|'s view to the web controller's content view.
  void ShowWebInterstitial(WebInterstitialImpl* interstitial);

  // Called to dismiss the currently-displayed transient content view.
  void ClearTransientContentView();

  // Notifies the delegate that the load progress was updated.
  void SendChangeLoadProgress(double progress);
  // Notifies the delegate that a context menu needs handling.
  bool HandleContextMenu(const ContextMenuParams& params);

  // Notifies the delegate that a Form Repost dialog needs to be presented.
  void ShowRepostFormWarningDialog(const base::Callback<void(bool)>& callback);

  // Notifies the delegate that a JavaScript dialog needs to be presented.
  void RunJavaScriptDialog(const GURL& origin_url,
                           JavaScriptDialogType java_script_dialog_type,
                           NSString* message_text,
                           NSString* default_prompt_text,
                           const DialogClosedCallback& callback);

  // Notifies the delegate that request receives an authentication challenge
  // and is unable to respond using cached credentials.
  void OnAuthRequired(NSURLProtectionSpace* protection_space,
                      NSURLCredential* proposed_credential,
                      const WebStateDelegate::AuthCallback& callback);

  // Cancels all dialogs associated with this web_state.
  void CancelDialogs();

  // NavigationManagerDelegate:
  void GoToIndex(int index) override;
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
  // The SessionStorageBuilder functions require access to private variables of
  // WebStateImpl.
  friend SessionStorageBuilder;

  // Creates a WebUIIOS object for |url| that is owned by the caller. Returns
  // nullptr if |url| does not correspond to a WebUI page.
  std::unique_ptr<web::WebUIIOS> CreateWebUIIOS(const GURL& url);

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

  // The NavigationManagerImpl that stores session info for this WebStateImpl.
  std::unique_ptr<NavigationManagerImpl> navigation_manager_;

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

  // Callbacks associated to command prefixes.
  std::map<std::string, ScriptCommandCallback> script_command_callbacks_;

  // Member variables should appear before the WeakPtrFactory<> to ensure that
  // any WeakPtrs to WebStateImpl are invalidated before its member variable's
  // destructors are executed, rendering them invalid.
  base::WeakPtrFactory<WebState> weak_factory_;

  // Mojo interface registry for this WebState.
  std::unique_ptr<service_manager::InterfaceRegistry> mojo_interface_registry_;

  DISALLOW_COPY_AND_ASSIGN(WebStateImpl);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_WEB_STATE_IMPL_H_
