# Chrome Network Bug Triage : Labels

## Some network label caveats

**Cr-UI-Browser-Downloads**
: Despite the name, this covers all issues related to downloading a file except
  saving entire pages (which is **Cr-Blink-SavePage**), not just UI issues.
  Most downloads bugs will have the word "download" or "save as" in the
  description.  Issues with the HTTP server for the Chrome binaries are not
  downloads bugs.

**Cr-UI-Browser-SafeBrowsing**
: Bugs that have to do with the process by which a URL or file is determined to
  be dangerous based on our databases, or the resulting interstitials.
  Determination of danger based purely on content-type or file extension
  belongs in **Cr-UI-Browser-Downloads**, not SafeBrowsing.

**Cr-Internals-Network-SSL**
: This includes issues that should be also tagged as **Cr-Security-UX**
  (certificate error pages or other security interstitials, omnibox indicators
  that a page is secure), and more general SSL issues.  If you see requests
  that die in the SSL negotiation phase, in particular, this is often the
  correct label.

**Cr-Internals-Network-DataProxy**
: Flywheel / the Data Reduction Proxy.  Issues require "Reduce Data Usage" be
  turned on.  Proxy url is [https://proxy.googlezip.net:443](), with
  [http://compress.googlezip.net:80]() as a fallback.  Currently Android and
  iOS only.

**Cr-Internals-Network-Cache**
: The cache is the layer that handles most range request logic (Though range
  requests may also be issued by the PDF plugin, XHRs, or other components).

**Cr-Internals-Network-SPDY**
: Covers HTTP2 as well.

**Cr-Internals-Network-HTTP**
: Typically not used.  Unclear what it covers, and there's no specific HTTP
  owner.

**Cr-Internals-Network-Logging**
: Covers **about:net-internals**, **about:net-export** as well as the what's
  sent to the NetLog.

**Cr-Internals-Network-Connectivity**
: Issues related to switching between networks, ERR_NETWORK_CHANGED, Chrome
  thinking it's online when it's not / navigator.onLine inaccuracies, etc.

**Cr-Internals-Network-Filters**
: Covers SDCH and gzip issues.  ERR_CONTENT_DECODING_FAILED indicates a problem
  at this layer, and bugs here can also cause response body corruption.

## Common non-network labels

Bugs in these areas often receive the **Cr-Internals-Network** label, though
they fall largely outside the purview of the network stack team:

**Cr-Blink-Forms**
: Issues submitting forms, forms having weird data, forms sending the wrong
  method, etc.

**Cr-Blink-Loader**
: Cross origin issues are sometimes loader related.  Blink also has an
  in-memory cache, and when it's used, requests don't appear in
  about:net-internals.  Requests for the same URL are also often merged there
  as well.  This does *not* cover issues with content/browser/loader/ files.

**Cr-Blink-ServiceWorker**

**Cr-Blink-Storage-AppCache**

**Cr-Blink-Network-WebSockets**
: Issues with the WebSockets.  Attach this label to any issue about the
  WebSocket feature regardless of where the cause of the issue is (net/ or
  Blink).

**Cr-Blink-Network-FetchAPI**
: Generic issues with the Fetch API - missing request or response
  headers, multiple headers, etc.  These will often run into issues in certain
  corner cases (Cross origin / CORS, proxy, whatever).  Attach all labels that
  seem appropriate.

**Cr-Blink-Network-XHR**
: Generic issues with sync/async XHR requests.

**Cr-Services-Sync**
: Sharing data/tabs/history/passwords/etc between machines not working.

**Cr-Services-Chromoting**

**Cr-Platform-Extensions**
: Issues extensions loading / not loading / hanging.

**Cr-Platform-Extensions-API**
: Issues with network related extension APIs should have this label.
  chrome.webRequest is the big one, I believe, but there are others.

**Cr-Internals-Plugins-Pepper[-SDK]**

**Cr-UI-Browser-Omnibox**
: Basically any issue with the omnibox.  URLs being treated as search queries
  rather than navigations, dropdown results being weird, not handling certain
  unicode characters, etc.  If the issue is new TLDs not being recognized by
  the omnibox, that's due to Chrome's TLD list being out of date, and not an
  omnibox issue.  Such TLD issues should be duped against
  http://crbug.com/37436.

**Cr-Internals-Media-Network**
: Issues related to media.  These often run into the 6 requests per hostname
  issue, and also have fun interactions with the cache, particularly in the
  range request case.

**Cr-Internals-Plugins-PDF**
: Issues loading pdf files.  These are often related to range requests, which
  also have some logic at the Internals-Network-Cache layer.

**Cr-UI-Browser-Navigation**

**Cr-UI-Browser-History**
: Issues which only appear with forward/back navigation.

**Cr-OS-Systems-Network** / **Cr-OS-Systems-Mobile** / **Cr-OS-Systems-Bluetooth**
: These should be used for issues with ChromeOS's platform network code, and
  not net/ issues on ChromeOS.

**Cr-Blink-SecurityFeature**
: CORS / Cross origin issues.  Main frame cross-origin navigation issues are
  often actually **Cr-UI-Browser-Navigation** issues.

**Cr-Privacy**
: Privacy related bug (History, cookies discoverable by an entity that
  shouldn't be able to do so, incognito state being saved in memory or on disk
  beyond the lifetime of incognito tabs, etc).  Generally used in conjunction
  with other labels.

**Type-Bug-Security**
: Security related bug (Allows for code execution from remote site, allows
  crossing security boundaries, unchecked array bounds,
  etc).
