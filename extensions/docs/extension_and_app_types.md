# Extension and App Types

Generally, browser extensions cut across websites and web apps, while apps
provide more isolated functionality.

[TOC]

![Summary of extension types showing browser extensions, packaged/platform apps,
and hosted/bookmark apps](extension_types.png)

## Browser extensions

Browser extensions often provide an interactive toolbar icon, but can also run
without any UI. They may interact with the browser or tab contents, and can
request more extensive permissions than apps.

## Apps

### Platform app

Platform apps (*v2 packaged apps*) are standalone applications that mostly run
independently of the browser. Their windows look and feel like native
applications but simply host the app's pages.

Most apps, like Calculator and the Files app, create their window(s) and
initialize a UI in response to Chrome's `chrome.app.runtime.onLaunched` event.
Some apps don't show a window but work in the background instead. Platform apps
can connect to more device types than browser extensions have access to.

Platform apps are deprecated on non-Chrome OS platforms.

### Packaged app (legacy)

[Legacy (v1) packaged apps](https://developer.chrome.com/extensions/apps)
combined the appearance of a [hosted app](#Hosted-app) -- a windowed wrapper
around a website -- with the power of extension APIs. With the launch of
platform apps and the app-specific APIs, legacy packaged apps are deprecated.

### Hosted app

A [hosted app](https://developer.chrome.com/webstore/hosted_apps) is mostly
metadata: a web URL to launch, a list of associated URLs, and a list of HTML5
permissions. Chrome ask for these permissions during the app's installation,
allowing the associated URL to bypass the normal Chrome permission prompts for
HTML5 features.

### Bookmark app

A bookmark app is a simplified hosted app that Chrome creates on demand. When
the user taps "Add to desktop" (or "Add to shelf" on Chrome OS) in the Chrome
menu, Chrome creates a barebones app whose manifest specifies the current tab's
URL. A shortcut to this URL appears in chrome://apps using the site's favicon.

Chrome then creates a desktop shortcut that will open a browser window with
flags that specify the app and profile. Activating the icon launches the
"bookmarked" URL in a tab or a window.

## Manifest

A particular manifest key in an extension's `manifest.json` file determines what
kind of extension it is:

* Platform app: `app.background.page` and/or `app.background.scripts`
* Legacy packaged app: `app.launch.local_path`
* Hosted app: `app.launch.web_url`
* Browser extension: none of the above

## Notes

`Extension` is the class type for all extensions and apps, so technically
speaking, an app *is-an* `Extension`. The word "extension" usually refers only
to non-app extensions, a.k.a. *browser extensions*.

## See also

* [`Manifest::Type` declaration](https://cs.chromium.org/chromium/src/extensions/common/manifest.h?gs=cpp%253Aextensions%253A%253Aclass-Manifest%253A%253Aenum-Type%2540chromium%252F..%252F..%252Fextensions%252Fcommon%252Fmanifest.h%257Cdef&gsn=Type&ct=xref_usages)
* Extensions (3rd-party developer documentation)
    * [Extension APIs](https://developer.chrome.com/extensions/api_index)
    * [Extension manifest file format](
      https://developer.chrome.com/extensions/manifest)
* Apps (3rd-party developer documentation)
    * [Platform app APIs](https://developer.chrome.com/apps/api_index)
    * [Platform app manifest file format](
      https://developer.chrome.com/apps/manifest)
    * [Choosing an app type](https://developer.chrome.com/webstore/choosing)
    * Ancient article introducing the [motivation for apps (outdated)](
      https://developer.chrome.com/webstore/apps_vs_extensions)
