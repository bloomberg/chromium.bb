// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/applescript/tab_applescript.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#import "base/memory/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/cocoa/applescript/apple_event_util.h"
#include "chrome/browser/ui/cocoa/applescript/error_applescript.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/save_page_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "googleurl/src/gurl.h"

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderViewHost;
using content::Referrer;
using content::WebContents;

namespace {

void ResumeAppleEventAndSendReply(NSAppleEventManagerSuspensionID suspension_id,
                                  const base::Value* result_value) {
  NSAppleEventDescriptor* result_descriptor =
      chrome::mac::ValueToAppleEventDescriptor(result_value);

  NSAppleEventManager* manager = [NSAppleEventManager sharedAppleEventManager];
  NSAppleEventDescriptor* reply_event =
      [manager replyAppleEventForSuspensionID:suspension_id];
  [reply_event setParamDescriptor:result_descriptor
                       forKeyword:keyDirectObject];
  [manager resumeWithSuspensionID:suspension_id];
}

}  // namespace

@interface TabAppleScript()
@property (nonatomic, copy) NSString* tempURL;
@end

@implementation TabAppleScript

@synthesize tempURL = tempURL_;

- (id)init {
  if ((self = [super init])) {
    SessionID session;
    SessionID::id_type futureSessionIDOfTab = session.id() + 1;
    // Holds the SessionID that the new tab is going to get.
    scoped_nsobject<NSNumber> numID(
        [[NSNumber alloc] initWithInt:futureSessionIDOfTab]);
    [self setUniqueID:numID];
  }
  return self;
}

- (void)dealloc {
  [tempURL_ release];
  [super dealloc];
}

- (id)initWithWebContents:(content::WebContents*)webContents {
  if (!webContents) {
    [self release];
    return nil;
  }

  if ((self = [super init])) {
    // It is safe to be weak; if a tab goes away (e.g. the user closes a tab)
    // the AppleScript runtime calls tabs in AppleScriptWindow and this
    // particular tab is never returned.
    webContents_ = webContents;
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(webContents);
    scoped_nsobject<NSNumber> numID(
        [[NSNumber alloc] initWithInt:session_tab_helper->session_id().id()]);
    [self setUniqueID:numID];
  }
  return self;
}

- (void)setWebContents:(content::WebContents*)webContents {
  DCHECK(webContents);
  // It is safe to be weak; if a tab goes away (e.g. the user closes a tab)
  // the AppleScript runtime calls tabs in AppleScriptWindow and this
  // particular tab is never returned.
  webContents_ = webContents;
  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(webContents);
  scoped_nsobject<NSNumber> numID(
      [[NSNumber alloc] initWithInt:session_tab_helper->session_id().id()]);
  [self setUniqueID:numID];

  if ([self tempURL])
    [self setURL:[self tempURL]];
}

- (NSString*)URL {
  if (!webContents_) {
    return nil;
  }

  NavigationEntry* entry = webContents_->GetController().GetActiveEntry();
  if (!entry) {
    return nil;
  }
  const GURL& url = entry->GetVirtualURL();
  return base::SysUTF8ToNSString(url.spec());
}

- (void)setURL:(NSString*)aURL {
  // If a scripter sets a URL before the node is added save it at a temporary
  // location.
  if (!webContents_) {
    [self setTempURL:aURL];
    return;
  }

  GURL url(base::SysNSStringToUTF8(aURL));
  // check for valid url.
  if (!url.is_empty() && !url.is_valid()) {
    AppleScript::SetError(AppleScript::errInvalidURL);
    return;
  }

  NavigationEntry* entry = webContents_->GetController().GetActiveEntry();
  if (!entry)
    return;

  const GURL& previousURL = entry->GetVirtualURL();
  webContents_->OpenURL(OpenURLParams(
      url,
      content::Referrer(previousURL, WebKit::WebReferrerPolicyDefault),
      CURRENT_TAB,
      content::PAGE_TRANSITION_TYPED,
      false));
}

- (NSString*)title {
  NavigationEntry* entry = webContents_->GetController().GetActiveEntry();
  if (!entry)
    return nil;

  string16 title = entry ? entry->GetTitle() : string16();
  return base::SysUTF16ToNSString(title);
}

- (NSNumber*)loading {
  BOOL loadingValue = webContents_->IsLoading() ? YES : NO;
  return [NSNumber numberWithBool:loadingValue];
}

- (void)handlesUndoScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Undo();
}

- (void)handlesRedoScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Redo();
}

- (void)handlesCutScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Cut();
}

- (void)handlesCopyScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Copy();
}

- (void)handlesPasteScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Paste();
}

- (void)handlesSelectAllScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return;
  }

  view->SelectAll();
}

- (void)handlesGoBackScriptCommand:(NSScriptCommand*)command {
  NavigationController& navigationController = webContents_->GetController();
  if (navigationController.CanGoBack())
    navigationController.GoBack();
}

- (void)handlesGoForwardScriptCommand:(NSScriptCommand*)command {
  NavigationController& navigationController = webContents_->GetController();
  if (navigationController.CanGoForward())
    navigationController.GoForward();
}

- (void)handlesReloadScriptCommand:(NSScriptCommand*)command {
  NavigationController& navigationController = webContents_->GetController();
  const bool checkForRepost = true;
  navigationController.Reload(checkForRepost);
}

- (void)handlesStopScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    // We tolerate Stop being called even before a view has been created.
    // So just log a warning instead of a NOTREACHED().
    DLOG(WARNING) << "Stop: no view for handle ";
    return;
  }

  view->Stop();
}

- (void)handlesPrintScriptCommand:(NSScriptCommand*)command {
  bool initiated =
      printing::PrintViewManager::FromWebContents(webContents_)->PrintNow();
  if (!initiated) {
    AppleScript::SetError(AppleScript::errInitiatePrinting);
  }
}

- (void)handlesSaveScriptCommand:(NSScriptCommand*)command {
  NSDictionary* dictionary = [command evaluatedArguments];

  NSURL* fileURL = [dictionary objectForKey:@"File"];
  // Scripter has not specifed the location at which to save, so we prompt for
  // it.
  if (!fileURL) {
    webContents_->OnSavePage();
    return;
  }

  base::FilePath mainFile(base::SysNSStringToUTF8([fileURL path]));
  // We create a directory path at the folder within which the file exists.
  // Eg.    if main_file = '/Users/Foo/Documents/Google.html'
  // then directory_path = '/Users/Foo/Documents/Google_files/'.
  base::FilePath directoryPath = mainFile.RemoveExtension();
  directoryPath = directoryPath.InsertBeforeExtension(std::string("_files/"));

  NSString* saveType = [dictionary objectForKey:@"FileType"];

  content::SavePageType savePageType = content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML;
  if (saveType) {
    if ([saveType isEqualToString:@"only html"]) {
      savePageType = content::SAVE_PAGE_TYPE_AS_ONLY_HTML;
    } else if ([saveType isEqualToString:@"complete html"]) {
      savePageType = content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML;
    } else {
      AppleScript::SetError(AppleScript::errInvalidSaveType);
      return;
    }
  }

  webContents_->SavePage(mainFile, directoryPath, savePageType);
}

- (void)handlesCloseScriptCommand:(NSScriptCommand*)command {
  webContents_->GetDelegate()->CloseContents(webContents_);
}

- (void)handlesViewSourceScriptCommand:(NSScriptCommand*)command {
  NavigationEntry* entry =
      webContents_->GetController().GetLastCommittedEntry();
  if (entry) {
    webContents_->OpenURL(OpenURLParams(
        GURL(chrome::kViewSourceScheme + std::string(":") +
             entry->GetURL().spec()),
        Referrer(),
        NEW_FOREGROUND_TAB,
        content::PAGE_TRANSITION_LINK,
        false));
  }
}

- (id)handlesExecuteJavascriptScriptCommand:(NSScriptCommand*)command {
  RenderViewHost* view = webContents_->GetRenderViewHost();
  if (!view) {
    NOTREACHED();
    return nil;
  }

  NSAppleEventManager* manager = [NSAppleEventManager sharedAppleEventManager];
  NSAppleEventManagerSuspensionID suspensionID =
      [manager suspendCurrentAppleEvent];
  content::RenderViewHost::JavascriptResultCallback callback =
      base::Bind(&ResumeAppleEventAndSendReply, suspensionID);

  string16 script = base::SysNSStringToUTF16(
      [[command evaluatedArguments] objectForKey:@"javascript"]);
  view->ExecuteJavascriptInWebFrameCallbackResult(string16(),  // frame_xpath
                                                  script,
                                                  callback);

  return nil;
}

@end
