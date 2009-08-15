// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef CHROME_PERSONALIZATION

// TODO(timsteele): Remove this file by finding proper homes for everything in
// trunk.
#ifndef CHROME_BROWSER_SYNC_PERSONALIZATION_H_
#define CHROME_BROWSER_SYNC_PERSONALIZATION_H_

#include <string>
#include "base/basictypes.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"

class Browser;
class DOMUI;
class DOMMessageHandler;
class Profile;
class RenderView;
class RenderViewHost;
class WebFrame;
class WebView;

class ProfileSyncService;
class ProfileSyncServiceObserver;

namespace views { class View; }

// Contains methods to perform Personalization-related tasks on behalf of the
// caller.
namespace Personalization {

// Returns whether |url| should be loaded in a DOMUI.
bool NeedsDOMUI(const GURL& url);

// Handler for "about:sync"
std::string AboutSync();

// Construct a new DOMMessageHandler for the new tab page |dom_ui|.
DOMMessageHandler* CreateNewTabPageHandler(DOMUI* dom_ui);

// Get HTML for the Personalization iframe in the New Tab Page.
std::string GetNewTabSource();

// Returns the text for personalization info menu item and sets its enabled
// state.
std::wstring GetMenuItemInfoText(Browser* browser);

// Performs appropriate action when the sync menu item is clicked.
void HandleMenuItemClick(Profile* p);
}  // namespace Personalization

#endif  // CHROME_BROWSER_SYNC_PERSONALIZATION_H_
#endif  // CHROME_PERSONALIZATION
