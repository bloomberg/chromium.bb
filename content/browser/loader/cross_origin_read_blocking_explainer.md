# Cross-Origin Read Blocking (CORB)

This document outlines Cross-Origin Read Blocking, an algorithm by which some
dubious cross-origin resource loads may be identified and blocked by web
browsers before they reach the web page. CORB offers a way to maintain
same-origin protections on user data, even in the presence of side channel
attacks.


### The problem

The same-origin policy generally prevents one origin from reading arbitrary
network resources from another origin. In practice, enforcing this policy is not
as simple as blocking all cross-origin loads: exceptions must be established for
web features, like `<img>` or `<script>`, which can target cross-origin
resources for historical reasons, and for the CORS mechanism, which allows some
resources to be selectively read across origins.

Certain type of content, however, can be shown to be incompatible with all of
the historically-allowed permissive contexts. JSON is one such type: a JSON
response will result in a decode error when targeted by the `<img>` tag, either
a no-op or syntax error when targeted by the `<script>` tag, and so on. The only
case where a web page can load JSON with observable consequences, is via
fetch() or XMLHttpRequest; and in those cases, cross-origin reads are moderated
by CORS.

If such resources can be detected and blocked earlier in the progress of the
load -- say, before the response makes it to the image decoder or JavaScript
parser -- the chances of information leakage via potential side channels can be
reduced.


### What attacks can CORB mitigate?

CORB can mitigate the following attack vectors:

* Cross-Site Script Inclusion (XSSI)
  * XSSI is the technique of pointing the `<script>` tag at a target resource
    which is not JavaScript, and observing some side-effects when the resulting
    resource is interpreted as JavaScript. An early example of this attack was
    discovered in 2006: by overwriting the Javascript Array constructor, the
    contents of JSON lists could be intercepted as simply as:
      `<script src="https://example.com/secret.json">`.
    While the array constructor attack vector is fixed in current
    browsers, numerous similar exploits have been found and fixed in the
    subsequent decade. For example, see the
    [slides here](https://www.owasp.org/images/6/6a/OWASPLondon20161124_JSON_Hijacking_Gareth_Heyes.pdf).
  * CORB prevents this class of attacks, because a CORB-protected resource will
    be blocked from ever being delivered to a cross-site `<script>` element.
  * CORB is particularly valuable in absence of other XSSI defenses like XSRF
    tokens and/or JSON parser breakers.  The presence of other XSSI
    defenses like JSON parser breakers can be used as a signal to the CORB
    algorithm that a resource should be CORB-protected.

* Speculative Side Channel Attack (e.g. Spectre).
  * For example, an attacker may 1) use an
    `<img src="https://example.com/secret.json">` element to pull a cross-site
    secret into the process where the attacker's Javascript runs, and then 2)
    use a speculative side channel attack (e.g. Spectre) to read the secret.
  * CORB can prevent this class of attacks when used in tandem with Site
    Isolation, by preventing the JSON resource from being present in the
    memory of a process hosting a cross-site page.


### Algorithm Goals

* CORB-protected resources should be blocked from reaching cross-site pages,
  unless explicitly permitted by CORS.
* CORB should protect as many resources as possible.
* CORB should cause minimal web-compatibility breakages.
* CORB should be resilient to the most frequent Content-Type mislabelings,
  unless the site owner asserts that the response is labelled correctly via
  `X-Content-Type-Options: nosniff` header.
* CORB should provide a means for web authors to affirmatively protect resources
  which are not intended for cross-origin access.


### How does CORB "block" a response?

When CORB decides that a response needs to be CORB-protected, the response is
modified as follows:
* The response response body is replaced with an empty body.
* The response headers are filtered down to the
  [CORS safelisted response headers](https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name)
  (i.e. all other response headers are removed from the response).

CORB blocking needs to take place before the response reaches the process
hosting the cross-origin initiator of the request.  In other words, CORB
blocking should prevent CORB-protected response data from ever being present in
the memory of the process hosting a cross-origin website (even temporarily or
for a short term).  This is different from the concept of
[filtered responses](https://fetch.spec.whatwg.org/#concept-filtered-response) (e.g.
[CORS filtered response](https://fetch.spec.whatwg.org/#concept-filtered-response-cors) or
[opaque filtered response](https://fetch.spec.whatwg.org/#concept-filtered-response-opaque))
which just provide a limited view into full data that remains stored in an
[internal response](https://fetch.spec.whatwg.org/#concept-internal-response)
and may be implemented inside the renderer process.


### What kinds of requests are CORB-eligible?

Responses to the following kinds of requests are not eligible for CORB
protection:

* [navigation requests](https://fetch.spec.whatwg.org/#navigation-request)
  or requests where the
  [request destination](https://fetch.spec.whatwg.org/#concept-request-destination)
  is "object" or "embed".
  Enforcing isolation between cross-origin `<iframe>`s or `<object>`s or is
  outside the scope of CORB (and depends on Site Isolation approach specific to
  each browser).

> [lukasza@chromium.org] TODO: Figure out how
> [Edge's VM-based isolation](https://cloudblogs.microsoft.com/microsoftsecure/2017/10/23/making-microsoft-edge-the-most-secure-browser-with-windows-defender-application-guard/)
> works (e.g. if some origins are off-limits in particular renderers, then this
> would greatly increase utility of CORB in Edge).

> [lukasza@chromium.org] TODO: Figure out how other browsers approach Site
> Isolation (e.g. even if there is no active work, maybe there are some bugs we
> can reference here).

* Download requests (e.g. requests where the
  [initiator](https://fetch.spec.whatwg.org/#concept-request-initiator)
  is "download").  In this case the data from the response is saved to disk
  (instead of being shared to a cross-origin context) and therefore wouldn't
  benefit from CORB protection.

> [lukasza@chromium.org] AFAIK, in Chrome a response to a download request never
> passes through memory of a renderer process.  Not sure if this is true in
> other browsers.

All other kinds of requests may be CORB-eligible.  This includes:
* [XHR](https://xhr.spec.whatwg.org/)
  and [fetch()](https://fetch.spec.whatwg.org/#dom-global-fetch)
* `ping`, `navigator.sendBeacon()`
* `<link rel="prefetch" ...>`
* Requests with the following
  [request destination](https://fetch.spec.whatwg.org/#concept-request-destination):
    - "image" requests like `<img>` tag, `/favicon.ico`, SVG's `<image>`,
      CSS' `background-image`, etc.
    - [script-like destinations](https://fetch.spec.whatwg.org/#request-destination-script-like)
      like `<script>`, `importScripts()`, `navigator.serviceWorker.register()`,
      `audioWorklet.addModule()`, etc.
    - "audio", "video" or "track"
    - "font"
    - "style"
    - "report" requests like CSP reports, NEL reports, etc.


### What assets can CORB protect?

The CORB algorithm will declare a response as either CORB-protected (and
eligible to be blocked) or CORB-exempt (and always allowed through). It will be
shown that the following types of content can be CORB-protected:
 * JSON
 * XML
 * HTML (with special handling of `<iframe>` embedding and )
 * Plain text
 * Any response, when prefixed with certain XSSI-defeating patterns (this is
   a common convention for conveying JSON).

> [lukasza@chromium.org] TODO: Rewrite the remainder of this section:
> - Change *document*-vs-*resource* narrative to
>   CORB-protected VS CORB-allowed (or non-CORB-eligible).
> - Plain text = sniffing for HTML or XML or JSON
> - Exclude PDF/ZIP/other from CORB and cover how web developers can protect
>   PDF/ZIP/other resources even though the are not CORB-protected
> - Cover how images/videos are not protected
>   (mention possibility of an opt-in via header)

CORB decides whether a response is a *document* or a *resource* primarily based
on the Content-Type header.

CORB considers responses with the following `Content-Type` headers to be
*resources* (these are the content types that can be used in `<img>`, `<audio>`,
`<video>`, `<script>` and other similar elements which may be embedded
cross-origin):
* [JavaScript MIME type](https://html.spec.whatwg.org/#javascript-mime-type)
  like `application/javascript` or `text/jscript`
* `text/css`
* [image types](https://mimesniff.spec.whatwg.org/#image-type) like types
  matching `image/*`
* [audio or video types](https://mimesniff.spec.whatwg.org/#audio-or-video-type)
  like `audio/*`, `video/*` or `application/ogg`
* [text/vtt](https://w3c.github.io/webvtt/#file-structure)
* `font/*` or one of legacy
  [font types](https://mimesniff.spec.whatwg.org/#font-type)

> [lukasza@chromium.org] Some images (and other content types) may contain
> sensitive data that shouldn't be shared with other origins.  To avoid breaking
> existing websites images have to be treated by default as cross-origin
> *resources*, but maybe we should consider letting websites opt-into additional
> protection.  For examples a server might somehow indicate to treat its images
> as origin-bound *documents* protected by CORB (e.g.  with a new kind of HTTP
> response header that we might want to consider).

CORB considers HTML, XML and JSON to be *documents* - this covers responses
with one of the following `Content-Type` headers:
* [HTML MIME type](https://mimesniff.spec.whatwg.org/#html-mime-type)
* [XML MIME type](https://mimesniff.spec.whatwg.org/#xml-mime-type)
  (except `image/svg+xml` which is a *resource*)
* [JSON MIME type](https://html.spec.whatwg.org/#json-mime-type)

Responses marked as `multipart/*` are conservatively considered *resources*.
This avoids having to parse the content types of the nested parts.
We recommend not supporting multipart range requests for sensitive documents.

Responses with a MIME type not explicitly named above (e.g. `application/pdf` or
`application/zip`) are considered to be *documents*.  Similarly, responses that
don't contain a `Content-Type` header, are also considered *documents*.  This
helps meet the goal of protecting as many *documents* as possible.

> [lukasza@chromium.org] Maybe this is too aggressive for the initial launch of
> CORB?  See also https://crbug.com/802836.  OTOH, it seems that in the
> long-term this is the right approach (e.g. defining a short list of types and
> type patterns that don't need protection, rather than trying to define a long
> and open-ended list of types that need protection today or would need
> protection in the future).

CORB considers `text/plain` to be a *document*.
TODO: application/octet-stream.

> [lukasza@chromium.org] This seems like a one-off in the current
> implementation.  Maybe `text/plain` should just be treated as
> "a MIME type not explicitly named above".

Additionally CORB may classify as a *document* any response that 1) has
Content-Type set to a non-empty value different from `text/css` and 2) starts
with a JSON parser breaker (e.g. `)]}'` or `{}&&` or `for(;;);`) - regardless of
the presence of `X-Content-Type-Options: nosniff` header.  A JSON parser breaker
is highly unlikely to be present in a *resource* (and therefore highly unlikely
to lead to misclassification of a *resource* as a *document*) - for example:

* A JSON parser breaker would cause a syntax error (or a hang) if present
  in an `application/javascript`.
* A JSON parser breaker is highly unlikely at the beginning of binary
  *resources* like images, videos or fonts (which typically require
  the first few bytes to be hardcoded to a specific sequence - for example
  `FF D8 FF` for image/jpeg).
* `text/css` (and missing and/or empty Content-Type) is an exception, because
  these Content-Types are allowed for stylesheets and it is possible to
  construct a file that begins with a JSON parser break, but at the same time
  parses fine as a stylesheet - for example:
```css
)]}'
{}
h1 { color: red; }
```

### Sniffing for *resources* served with an incorrect MIME type.

CORB can't always rely solely on the MIME type of the HTTP response to
distinguish documents from resources, since the MIME type on network responses
is sometimes wrong.  For example, some HTTP servers return a JPEG image with
a `Content-Type` header incorrectly saying `text/html`.

To avoid breaking existing websites, CORB may attempt to confirm if the response
body really matches the CORB-protected Content-Type response header:

* CORB will only sniff to confirm the classification based on the `Content-Type`
  header (i.e. if the `Content-Type` header is `text/json` then CORB will sniff
  for JSON and will not sniff for HTML and/or XML).

* If some syntax elements are shared between CORB-protected and
  non-CORB-protected MIME types, then these elements have to be ignored by CORB
  sniffing.  For example, when sniffing for (CORB-protected) HTML, CORB ignores
  and skips HTML comments, because
  [they can also be present](http://www.ecma-international.org/ecma-262/6.0/#sec-html-like-comments)
  in (non-CORB-protected) JavaScript.  This is different from the
  [HTML sniffing rules](https://mimesniff.spec.whatwg.org/#rules-for-identifying-an-unknown-mime-type),
  used in other contexts.

> [lukasza@chromium.org] It is not practical to try teaching CORB about sniffing
> all possible types of *documents* like `application/pdf`, `application/zip`,
> etc.

> [lukasza@chromium.org] Some MIME types types are inherently not sniffable
> (for example `application/octet-stream`).

> [lukasza@chromium.org] TODO: Explain how text/plain sniffs for *any* of HTML,
> XML or JSON.  Also discuss whether text/plain+nosniff should be different
> from text/html+nosniff (the latter should be CORB-protected, not sure about
> the former, given the still not understood media blocking that happens with
> CORB).

CORB should trust the `Content-Type` header in scenarios where sniffing
shouldn't or cannot be done:

* When `X-Content-Type-Options: nosniff` header is present.

* When the response is a partial, 206 response.

> [lukasza@chromium.org] An alternative behavior would be to allow (instead of
> blocking) 206 responses that would be sniffable otherwise (so one of HTML, XML
> or JSON + not accompanied by a nosniff header).  This alternative behavior
> would decrease the risk of blocking mislabeled resources, but would increase
> the risk of not blocking documents that need protection (an attacker could
> just need to issue a range request - protection in this case would depend on
> whether 1) the response includes a nosniff header and/or 2) the server rejects
> range requests altogether).  Note that the alternative behavior doesn't help
> with mislabeled text/plain responses (see also https://crbug.com/801709).

> [lukasza@chromium.org] We believe that mislabeling as HTML, JSON or XML is
> most common.  TODO: are we able to back this up with some numbers?

> [lukasza@chromium.org] Note that range requests are typically not issued
> when making requests for scripts and/or stylesheets.

Sniffing is a best-effort heuristic and for best security results, we
recommend 1) marking responses with the correct Content-Type header and 2)
opting out of sniffing by using the `X-Content-Type-Options: nosniff` header.


### How CORB interacts with images?

CORB should have no observable impact on `<img>` tags unless the image resource
is both 1) mislabeled with an incorrect, non-image, CORB-protected Content-Type
and 2) served with the `X-Content-Type-Options: nosniff` response header.

Examples:

* **Correctly-labeled HTML document**
  * Resource used in an `<img>` tag:
    * Body: an HTML document
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no observable difference** in behavior with and without
    CORB.  The rendered image should be the same broken image when 1) attempting
    to render an html document as an image (without CORB) and 2) attempting to
    render an empty response as an image (when CORB blocks the response).
  * WPT test: `fetch/corb/img-html-correctly-labeled.sub.html`

* **Mislabeled image (with sniffing)**
  * Resource used in an `<img>` tag:
    * Body: an image
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no difference** in behavior with and without CORB.
    CORB will sniff that the response body is *not* actually a CORB-protected
    type and therefore will allow the response.
  * WPT test: `fetch/corb/img-png-mislabeled-as-html.sub.html`

* **Mislabeled image (nosniff)**
  * Resource used in an `<img>` tag:
    * Body: an image
    * `Content-Type: text/html`
    * `X-Content-Type-Options: nosniff`
  * Expected behavior: **observable difference** in behavior in presence of CORB.
    Because of the `nosniff` header, CORB will have to rely on the
    `Content-Type` header.  Because this response is mislabeled (the body is an
    image, but the `Content-Type` header says that it is a html document), CORB
    will incorrectly classify the response as requiring CORB-protection.
  * WPT test: `fetch/corb/img-png-mislabeled-as-html-nosniff.tentative.sub.html`

In addition to the HTML `<img>` tag, the examples above should apply to other
web features that consume images: `/favicon.ico`, SVG's `<image>`,
`background-image` in stylesheets, painting images onto (potentially tainted)
HTML's `<canvas>`, etc.

> [lukasza@chromium.org] TODO: Figure out if we can measure and share how many
> of CORB-blocked responses are 1) for `<img>` tag, 2) `nosniff`, 3) 200-or-206
> status code, 4) non-zero (or non-available) Content-Length.  Cursory manual
> testing on major websites indicates that most CORB-blocked images are tracking
> pixels and therefore blocking them won't have any observable effect.

> [lukasza@chromium.org] Earlier attempts to block nosniff images with
> incompatible MIME types
> [failed](https://github.com/whatwg/fetch/issues/395).
> We think that CORB will succeed, because
> 1) it will only block a subset of CORB-protected MIME types
>    (e.g. it won't block `application/octet-stream` quoted
>    in a
>    [Firefox bug](https://bugzilla.mozilla.org/show_bug.cgi?id=1302539))
> 2) CORB is an important response to the recent announcement of new
>    side-channel attacks like Spectre.


### How CORB interacts with other multimedia?

TODO.

* TODO: audio/video - mostly like images, but note that 206 response is more
  likely (which today CORB treats as a nosniff signal - not sure if this is
  right: we need to block test/html+206+nosniff, but I am not sure if should
  block text/html+206 without nosniff - especially since we already recommend
  against supporting range requests for sensitive documents).

### How CORB interacts with scripts?

CORB should have no observable impact on `<script>` tags except for cases where
a CORB-protected, non-JavaScript resource labeled with its correct MIME type is
loaded as a script - in these cases the resource will usually result in a syntax
error, but CORB-protected response's empty body will result in no error.

Examples:

* **Correctly-labeled HTML document**
  * Resource used in a `<script>` tag:
    * Body: an HTML document
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **observable difference** via
    [GlobalEventHandlers.onerror](https://developer.mozilla.org/en-US/docs/Web/API/GlobalEventHandlers/onerror).
    Most CORB-protected resources would result in a syntax error when parsed as
    JavaScript.  The syntax error goes away after CORB blocks a response,
    because the empty body of the blocked response parses fine as JavaScript.
  * WPT test: `fetch/corb/script-html-correctly-labeled.tentative.sub.html`

> [lukasza@chromium.org] In theory, using a non-empty response in CORB-blocked
> responses might reintroduce the lost syntax error.  We didn't go down that
> path, because
> 1) using a non-empty response would be inconsistent with other parts of the
>    Fetch spec (like
>    [opaque filtered response](https://fetch.spec.whatwg.org/#concept-filtered-response-opaque)).
> 2) retaining the presence of the syntax error might require changing the
>    contents of a CORB-blocked response body depending on whether the original
>    response body would have caused a syntax error or not.  This would add
>    extra complexity that seems undesirable both for CORB implementors and for
>    web developers.

* **Mislabeled script (with sniffing)**
  * Resource used in a `<script>` tag:
    * Body: a proper JavaScript script
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no difference** in behavior with and without CORB.
    CORB will sniff that the response body is *not* actually a CORB-protected
    type and therefore will allow the response.  Note that CORB sniffing is
    resilient to the fact that some syntax elements are shared across HTML
    and JavaScript (e.g.
    [comments](http://www.ecma-international.org/ecma-262/6.0/#sec-html-like-comments)).
  * WPT test: `fetch/corb/script-js-mislabeled-as-html.sub.html`

* **Mislabeled script (nosniff)**
  * Resource used in a `<script>` tag:
    * Body: a proper JavaScript script
    * `Content-Type: text/html`
    * `X-Content-Type-Options: nosniff`
  * Expected behavior: **no observable difference** in behavior with and without CORB.
    Both with and without CORB, the script will not execute, because the
    `nosniff` response header response will cause the response to be blocked
    when its MIME type (`text/html` in the example) is not a
    [JavaScript MIME type](https://html.spec.whatwg.org/multipage/scripting.html#javascript-mime-type)
    (this behavior is required by the
    [Fetch spec](https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-nosniff?)).
  * WPT test: `fetch/corb/script-js-mislabeled-as-html-nosniff.sub.html`

In addition to the HTML `<script>` tag, the examples above should apply to other
web features that consume JavaScript including
[script-like destinations](https://fetch.spec.whatwg.org/#request-destination-script-like)
like `importScripts()`, `navigator.serviceWorker.register()`,
`audioWorklet.addModule()`, etc.


### How CORB interacts with stylesheets?

CORB should have no observable impact on stylesheets.

Examples:

*  **Anything not labeled as text/css**
  * Examples of resources used in a `<link rel="stylesheet" href="...">` tag:
    * Body: an HTML document OR a simple, valid stylesheet OR a polyglot
      HTML/CSS stylesheet
    * `Content-Type: text/html`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no observable difference** in behavior with and without
    CORB.  Even without CORB, such stylesheet examples will be rejected, because
    due to the
    [relaxed syntax rules](https://scarybeastsecurity.blogspot.dk/2009/12/generic-cross-browser-cross-domain.html)
    of CSS, cross-origin CSS requires a correct Content-Type header
    (restrictions vary by browser:
    [IE](http://msdn.microsoft.com/en-us/library/ie/gg622939%28v=vs.85%29.aspx),
    [Firefox](http://www.mozilla.org/security/announce/2010/mfsa2010-46.html),
    [Chrome](https://bugs.chromium.org/p/chromium/issues/detail?id=9877),
    [Safari](http://support.apple.com/kb/HT4070)
    (scroll down to CVE-2010-0051) and
    [Opera](http://www.opera.com/support/kb/view/943/)).
    This behavior is covered by
    [the HTML spec](https://html.spec.whatwg.org/#link-type-stylesheet)
    which 1) asks to only assume `text/css` Content-Type
    if the document embedding the stylesheet has been set to quirks mode and has
    the same origin and 2) only asks to run the steps for creating a CSS style
    sheet if Content-Type of the obtained resource is `text/css`.

  * WPT tests:
    `fetch/corb/style-css-mislabeled-as-html.sub.html`,
    `fetch/corb/style-html-correctly-labeled.sub.html`

*  **Anything not labeled as text/css (nosniff)**
  * Resource used in a `<link rel="stylesheet" href="...">` tag:
    * Body: a simple stylesheet
    * `Content-Type: text/html`
    * `X-Content-Type-Options: nosniff`
  * Expected behavior: **no observable difference** in behavior with and without CORB.
    Both with and without CORB, the stylesheet will not load, because the
    `nosniff` response header response will cause the response to be blocked
    when its MIME type (`text/html` in the example) is not `text/css`
    (this behavior is required by the
    [Fetch spec](https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-nosniff?)).
  * WPT test: `fetch/corb/style-css-mislabeled-as-html-nosniff.sub.html`

* **Correctly-labeled stylesheet with JSON parser breaker**
  * Resource used in a `<link rel="stylesheet" href="...">` tag:
    * Body: a stylesheet that begins with a JSON parser breaker
    * `Content-Type: text/css`
    * No `X-Content-Type-Options` header
  * Expected behavior: **no difference** in behavior with and without CORB,
    because CORB sniffing for JSON-parser-breakers is not performed for
    responses labeled as `Content-Type: text/css`.
  * WPT test: `fetch/corb/style-css-with-json-parser-breaker.sub.html`


### How CORB interacts with other web platform features?

CORB has no impact on the following scenarios:

* **[XHR](https://xhr.spec.whatwg.org/) and
  [fetch()](https://fetch.spec.whatwg.org/#dom-global-fetch)**
  * CORB has no observable effect, because [XHR](https://xhr.spec.whatwg.org/)
    and [fetch()](https://fetch.spec.whatwg.org/#dom-global-fetch) already apply
    same-origin policy to the responses (e.g. CORB should only block responses
    that would result in cross-origin XHR errors because of lack of CORS).

* **[Prefetch](https://html.spec.whatwg.org/#link-type-prefetch)**
  * CORB blocks response body from reaching a cross-origin renderer, but
    CORB doesn't prevent the response body from being cached by the browser
    process (and subsequently delivered into another, same-origin renderer
    process).

* **Tracking and reporting**
  * Various techniques try to check that a user has accessed some content
    by triggering a web request to a HTTP server that logs the user visit.
    The web request is frequently triggered by an invisible `img` element
    to a HTTP URI that usually replies either with a 204 or with a
    short HTML document.  In addition to the `img` tag, websites may use
    `style`, `script` and other tags to track usage.
  * CORB won't impact these techniques, as they don't depend on the actual
    content being returned by the HTTP server.  This also applies to other
    web features that don't care about the response: beacons, pings, CSP
    violation reports, etc.

* **Service workers**
  * Service workers may intercept cross-origin requests and artificially
    construct a response *within* the service worker (i.e. without crossing
    origins and/or security boundaries).  CORB will not block such responses.
  * When service workers cache actual cross-origin responses (e.g. in 'no-cors'
    request mode), the responses are 'opaque' and therefore CORB can block such
    responses without changing the service worker's behavior ('opaque' responses
    have a non-accessible body even without CORB).

* **Blob and File API**
  * Fetching cross-origin blob URLs is blocked even without CORB
    (see https://github.com/whatwg/fetch/issues/666).
  * WPT test: `script-html-via-cross-origin-blob-url.sub.html`
    (and also tests for navigation requests covered by the
    [commit here](https://github.com/mkruisselbrink/web-platform-tests/commit/9524a71919340eacc8aaa6e55ffe0b5aa72f9bfd)).


* **Content scripts and plugins**
  * TODO...

> [lukasza@chromium.org] TODO: Do we need to be more explicit about handling of
> requests initiated by plugins?
> - Should CORB attempt to intercept and parse
>   [crossdomain.xml](https://www.adobe.com/devnet/articles/crossdomain_policy_file_spec.html)
>   which tells Adobe Flash whether a particular cross-origin request is okay or
>   not (similarly to how CORB needs to understand CORS response headers)?
> - If CORB doesn't have knowledge about `crossdomain.xml`, then it will be
>   forced to allow all responses to Flash-initiated requests.  We should
>   clarify why CORB still provides security benefits in this scenario.
> - Also - not sure if plugin behavior is in-scope of
>   https://fetch.spec.whatwg.org?


### Demo page

To test CORB one can turn on the feature in Chrome M63+ by launching it with the
`--enable-features=CrossSiteDocumentBlockingAlways` cmdline flag.

CORB demo page: https://anforowicz.github.io/xsdb-demo/index.html


### Summary of CORB behavior

* Protected origins
  * CORB SHOULD allow same-origin responses.
  * CORB SHOULD block cross-origin responses from HTTP and/or HTTPS origins
    (this includes responses from `filesystem:` and `blob:` URIs if their nested
    origin has a HTTP and/or HTTPS scheme).
  * CORB MAY block cross-origin responses from non-HTTP/HTTPS origins.

> [lukasza@chromium.org] Should the filesystem/blob part be somehow weaved into
> one of explainer sections above?  WPT tests?

* Initiator origins
    * CORB SHOULD block responses for requests initiated from HTTP/HTTPS origins.
    * CORB SHOULD block responses for requests initiated from
      opaque/unique/sandboxed origins.
    * CORB MAY allow responses for requests initiated from `file:` origins.
    * CORB MAY allow responses for requests initiated from content scripts of
      browser extensions.

* Interoperability with other origin-related policies
    * CORB SHOULD allow responses that are otherwise allowed by CORS
    * CORB SHOULD allow non-opaque responses from service workers
    * CORB SHOULD block opaque responses from service workers

* *document*-vs-*resource* classification
    * CORB SHOULD classify as a *resource* all responses with the following
      Content-Type:
      * `application/javascript`
      * `text/css`
      * [image types](https://mimesniff.spec.whatwg.org/#image-type) - types
        matching `image/*`
      * [audio or video types](https://mimesniff.spec.whatwg.org/#audio-or-video-type)
        like `audio/*`, `video/*` or `application/ogg`
      * `font/*` or one of legacy
        [font types](https://mimesniff.spec.whatwg.org/#font-type)
    * CORB SHOULD classify as a *document* all responses with the following
      Content-Type if either 1) the response sniffs as the reported type or 2)
      the `X-Content-Type-Options: nosniff` header is present:
      * [HTML MIME type](https://mimesniff.spec.whatwg.org/#html-mime-type)
      * [XML MIME type](https://mimesniff.spec.whatwg.org/#xml-mime-type)
        (except `image/svg+xml` which is a *resource*)
      * JSON MIME type - one of `text/json`, `text/json+*`, `text/x-json`,
        `text/x-json+*`, `application/json`, `application/json+*` or `*+json`
    * CORB MAY classify as a *document* any response with a Content-Type not
      explicitly listed in the previous 2 bullet items.
    * CORB MAY classify as a *document* any response with a missing Content-Type
      response header.
    * CORB MAY classify as a *document* any response starting with a JSON parser
      breaker (e.g. `)]}'` or `{}&&` or `for(;;);`), regardless of its
      Content-Type and regardless of the presence of `X-Content-Type-Options:
      nosniff` header.

> [lukasza@chromium.org] Should the JSON parser breaker sniffing be somehow
> weaved into one of explainer sections above?

* Sniffing to confirm the Content-Type of the response
    * CORB SHOULD NOT sniff if `X-Content-Type-Options: nosniff` is present.
    * CORB MAY avoid sniffing 206 content range responses with a single-range.
    * CORB MAY limit sniffing to the first few network packets.
    * If Content-Type is `text/html` then CORB SHOULD allow the response
      if it doesn't
      [sniff as `text/html`](https://mimesniff.spec.whatwg.org/#rules-for-identifying-an-unknown-mime-type).
    * If Content-Type is one of TODO then CORB SHOULD allow the response
      if it doesn't
      [sniff as `text/xml`](https://mimesniff.spec.whatwg.org/#rules-for-identifying-an-unknown-mime-type).
    * If Content-Type is one of TODO then CORB SHOULD allow the response
      if it doesn't sniff as JSON.  TODO: define "sniff as JSON".


### Related specs

* https://fetch.spec.whatwg.org/#concept-filtered-response-opaque
* https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name
* https://fetch.spec.whatwg.org/#http-cors-protocol
* https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-nosniff?
* https://fetch.spec.whatwg.org/#x-content-type-options-header
* https://tools.ietf.org/html/rfc7233#section-4 (Responses to a Range Request)
