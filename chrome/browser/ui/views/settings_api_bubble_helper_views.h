// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SETTINGS_API_BUBBLE_HELPER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_SETTINGS_API_BUBBLE_HELPER_VIEWS_H_

struct AutocompleteMatch;
class Browser;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {

// Shows a bubble notifying the user that the homepage is controlled by an
// extension. This bubble is shown only on the first use of the Home button
// after the controlling extension takes effect.
void MaybeShowExtensionControlledHomeNotification(Browser* browser);

// Shows a bubble notifying the user that the search engine is controlled by an
// extension. This bubble is shown only on the first search after the
// controlling extension takes effect.
void MaybeShowExtensionControlledSearchNotification(
    Profile* profile,
    content::WebContents* web_contents,
    const AutocompleteMatch& match);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_SETTINGS_API_BUBBLE_HELPER_VIEWS_H_
