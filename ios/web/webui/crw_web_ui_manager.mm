// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/crw_web_ui_manager.h"

#include "base/json/string_escape.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/web/grit/ios_web_resources.h"
#import "ios/web/net/request_group_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/web_client.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/web_state/web_state_impl.h"
#import "ios/web/webui/crw_web_ui_page_builder.h"
#include "ios/web/webui/mojo_js_constants.h"
#include "ios/web/webui/url_fetcher_block_adapter.h"
#include "mojo/public/js/constants.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Prefix for history.requestFavicon JavaScript message.
const char kScriptCommandPrefix[] = "webui";
}

@interface CRWWebUIManager () <CRWWebUIPageBuilderDelegate>

// Current web state.
@property(nonatomic, readonly) web::WebStateImpl* webState;

// Composes WebUI page for webUIURL and invokes completionHandler with the
// result.
- (void)loadWebUIPageForURL:(const GURL&)webUIURL
          completionHandler:(void (^)(NSString*))completionHandler;

// Retrieves resource for URL and invokes completionHandler with the result.
- (void)fetchResourceWithURL:(const GURL&)URL
           completionHandler:(void (^)(NSData*))completionHandler;

// Handles JavaScript message from the WebUI page.
- (BOOL)handleWebUIJSMessage:(const base::DictionaryValue&)message;

// Handles webui.requestFavicon JavaScript message from the WebUI page.
- (BOOL)handleRequestFavicon:(const base::ListValue*)arguments;

// Handles webui.loadMojo JavaScript message from the WebUI page.
- (BOOL)handleLoadMojo:(const base::ListValue*)arguments;

// Executes mojo script and signals |webui.loadMojo| finish.
- (void)executeMojoScript:(const std::string&)mojoScript
            forModuleName:(const std::string&)moduleName
                   loadID:(const std::string&)loadID;

// Removes favicon callback from web state.
- (void)resetWebState;

// Removes fetcher from vector of active fetchers.
- (void)removeFetcher:(web::URLFetcherBlockAdapter*)fetcher;

@end

@implementation CRWWebUIManager {
  // Set of live WebUI fetchers for retrieving data.
  ScopedVector<web::URLFetcherBlockAdapter> _fetchers;
  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  // Weak WebStateImpl this CRWWebUIManager is associated with.
  web::WebStateImpl* _webState;
}

- (instancetype)init {
  NOTREACHED();
  return self;
}

- (instancetype)initWithWebState:(web::WebStateImpl*)webState {
  if (self = [super init]) {
    _webState = webState;
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
    base::WeakNSObject<CRWWebUIManager> weakSelf(self);
    _webState->AddScriptCommandCallback(
        base::BindBlock(
            ^bool(const base::DictionaryValue& message, const GURL&, bool) {
              return [weakSelf handleWebUIJSMessage:message];
            }),
        kScriptCommandPrefix);
  }
  return self;
}

- (void)dealloc {
  [self resetWebState];
}

#pragma mark - CRWWebStateObserver Methods

- (void)webState:(web::WebState*)webState
    didStartProvisionalNavigationForURL:(const GURL&)URL {
  DCHECK(webState == _webState);
  // If URL is not an application specific URL, ignore the navigation.
  if (!web::GetWebClient()->IsAppSpecificURL(URL))
    return;

  GURL navigationURL(URL);
  // Add request group ID to the URL, if not present. Request group ID may
  // already be added if restoring state to a WebUI page.
  GURL requestURL =
      web::ExtractRequestGroupIDFromURL(net::NSURLWithGURL(URL))
          ? URL
          : net::GURLWithNSURL(web::AddRequestGroupIDToURL(
                net::NSURLWithGURL(URL), _webState->GetRequestGroupID()));
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  [self loadWebUIPageForURL:requestURL
          completionHandler:^(NSString* HTML) {
            web::WebStateImpl* webState = [weakSelf webState];
            if (webState) {
              webState->LoadWebUIHtml(base::SysNSStringToUTF16(HTML),
                                      navigationURL);
            }
          }];
}

- (void)webStateDidLoadPage:(web::WebState*)webState {
  DCHECK_EQ(webState, _webState);
  // All WebUI pages are HTML based.
  _webState->SetContentsMimeType("text/html");
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self resetWebState];
}

#pragma mark - CRWWebUIPageBuilderDelegate Methods

- (void)webUIPageBuilder:(CRWWebUIPageBuilder*)webUIPageBuilder
    fetchResourceWithURL:(const GURL&)resourceURL
       completionHandler:(web::WebUIDelegateCompletion)completionHandler {
  GURL URL(resourceURL);
  [self fetchResourceWithURL:URL
           completionHandler:^(NSData* data) {
             base::scoped_nsobject<NSString> resource(
                 [[NSString alloc] initWithData:data
                                       encoding:NSUTF8StringEncoding]);
             completionHandler(resource, URL);
           }];
}

#pragma mark - Private Methods

- (void)loadWebUIPageForURL:(const GURL&)webUIURL
          completionHandler:(void (^)(NSString*))handler {
  base::scoped_nsobject<CRWWebUIPageBuilder> pageBuilder(
      [[CRWWebUIPageBuilder alloc] initWithDelegate:self]);
  [pageBuilder buildWebUIPageForURL:webUIURL completionHandler:handler];
}

- (void)fetchResourceWithURL:(const GURL&)URL
           completionHandler:(void (^)(NSData*))completionHandler {
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  web::URLFetcherBlockAdapterCompletion fetcherCompletion =
      ^(NSData* data, web::URLFetcherBlockAdapter* fetcher) {
        completionHandler(data);
        [weakSelf removeFetcher:fetcher];
      };

  _fetchers.push_back(
      [self fetcherForURL:URL completionHandler:fetcherCompletion]);
  _fetchers.back()->Start();
}

- (BOOL)handleWebUIJSMessage:(const base::DictionaryValue&)message {
  std::string command;
  if (!message.GetString("message", &command)) {
    DLOG(WARNING) << "Malformed message received";
    return NO;
  }
  const base::ListValue* arguments = nullptr;
  if (!message.GetList("arguments", &arguments)) {
    DLOG(WARNING) << "JS message parameter not found: arguments";
    return NO;
  }

  if (!arguments) {
    DLOG(WARNING) << "No arguments provided to " << command;
    return NO;
  }

  if (command == "webui.requestFavicon")
    return [self handleRequestFavicon:arguments];
  if (command == "webui.loadMojo")
    return [self handleLoadMojo:arguments];

  DLOG(WARNING) << "Unknown webui command received: " << command;
  return NO;
}

- (BOOL)handleRequestFavicon:(const base::ListValue*)arguments {
  std::string favicon;
  if (!arguments->GetString(0, &favicon)) {
    DLOG(WARNING) << "JS message parameter not found: Favicon URL";
    return NO;
  }
  GURL faviconURL(favicon);
  DCHECK(faviconURL.is_valid());
  // Retrieve favicon resource and set favicon background image via JavaScript.
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  void (^faviconHandler)(NSData*) = ^void(NSData* data) {
    base::scoped_nsobject<CRWWebUIManager> strongSelf(weakSelf);
    if (!strongSelf)
      return;
    NSString* base64EncodedResource = [data base64EncodedStringWithOptions:0];
    NSString* dataURLString = [NSString
        stringWithFormat:@"data:image/png;base64,%@", base64EncodedResource];
    NSString* faviconURLString = base::SysUTF8ToNSString(faviconURL.spec());
    NSString* script =
        [NSString stringWithFormat:@"chrome.setFaviconBackground('%@', '%@');",
                                   faviconURLString, dataURLString];
    [strongSelf webState]->ExecuteJavaScript(base::SysNSStringToUTF16(script));
  };
  [self fetchResourceWithURL:faviconURL completionHandler:faviconHandler];
  return YES;
}

- (BOOL)handleLoadMojo:(const base::ListValue*)arguments {
  std::string moduleName;
  if (!arguments->GetString(0, &moduleName)) {
    DLOG(WARNING) << "JS message parameter not found: Module name";
    return NO;
  }
  std::string loadID;
  if (!arguments->GetString(1, &loadID)) {
    DLOG(WARNING) << "JS message parameter not found: Load ID";
    return NO;
  }

  // Look for built-in scripts first.
  std::map<std::string, int> resource_map{
      {mojo::kBindingsModuleName, IDR_MOJO_BINDINGS_JS},
      {mojo::kBufferModuleName, IDR_MOJO_BUFFER_JS},
      {mojo::kCodecModuleName, IDR_MOJO_CODEC_JS},
      {mojo::kConnectionModuleName, IDR_MOJO_CONNECTION_JS},
      {mojo::kConnectorModuleName, IDR_MOJO_CONNECTOR_JS},
      {mojo::kRouterModuleName, IDR_MOJO_ROUTER_JS},
      {mojo::kUnicodeModuleName, IDR_MOJO_UNICODE_JS},
      {mojo::kValidatorModuleName, IDR_MOJO_VALIDATOR_JS},
      {web::kSyncMessageChannelModuleName,
       IDR_IOS_MOJO_SYNC_MESSAGE_CHANNEL_JS},
      {web::kHandleUtilModuleName, IDR_IOS_MOJO_HANDLE_UTIL_JS},
      {web::kSupportModuleName, IDR_IOS_MOJO_SUPPORT_JS},
      {web::kCoreModuleName, IDR_IOS_MOJO_CORE_JS},
      {web::kInterfaceProviderModuleName, IDR_IOS_SHELL_INTERFACE_PROVIDER_JS},
  };
  scoped_refptr<base::RefCountedMemory> scriptData(
      web::GetWebClient()->GetDataResourceBytes(resource_map[moduleName]));
  if (scriptData) {
    std::string script(reinterpret_cast<const char*>(scriptData->front()),
                       scriptData->size());
    [self executeMojoScript:script forModuleName:moduleName loadID:loadID];
    return YES;
  }

  // Not a built-in script, try retrieving from network.
  GURL resourceURL(self.webState->GetLastCommittedURL().Resolve(moduleName));
  base::WeakNSObject<CRWWebUIManager> weakSelf(self);
  [self fetchResourceWithURL:resourceURL completionHandler:^(NSData* data) {
    std::string script;
    if (data) {
      script = base::SysNSStringToUTF8(
          [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]);
      // WebUIIOSDataSourceImpl returns the default resource (which is the HTML
      // page itself) if the resource URL isn't found. Fail with error instead
      // of silently injecting garbage (leading to a not-very-useful syntax
      // error from the JS side).
      if (script.find("<!doctype") != std::string::npos) {
        NOTREACHED() << "cannot load " << moduleName;
        script.clear();
      }
    }

    [weakSelf executeMojoScript:script forModuleName:moduleName loadID:loadID];
  }];

  return YES;
}

- (void)executeMojoScript:(const std::string&)mojoScript
            forModuleName:(const std::string&)moduleName
                   loadID:(const std::string&)loadID {
  std::string script = mojoScript;
  // Append with completion function call.
  if (script.empty()) {
    DLOG(ERROR) << "Unable to find a module named " << moduleName;
    script = "__crWeb.webUIModuleLoadNotifier.moduleLoadFailed";
  } else {
    script += "__crWeb.webUIModuleLoadNotifier.moduleLoadCompleted";
  }
  base::StringAppendF(&script, "(%s, %s);",
                      base::GetQuotedJSONString(moduleName).c_str(),
                      base::GetQuotedJSONString(loadID).c_str());

  _webState->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

- (void)resetWebState {
  if (_webState) {
    _webState->RemoveScriptCommandCallback(kScriptCommandPrefix);
  }
  _webState = nullptr;
}

- (web::WebStateImpl*)webState {
  return _webState;
}

- (void)removeFetcher:(web::URLFetcherBlockAdapter*)fetcher {
  _fetchers.erase(std::find(_fetchers.begin(), _fetchers.end(), fetcher));
}

#pragma mark - Testing-Only Methods

- (std::unique_ptr<web::URLFetcherBlockAdapter>)
    fetcherForURL:(const GURL&)URL
completionHandler:(web::URLFetcherBlockAdapterCompletion)handler {
  return std::unique_ptr<web::URLFetcherBlockAdapter>(
      new web::URLFetcherBlockAdapter(
          URL, _webState->GetBrowserState()->GetRequestContext(), handler));
}

@end
