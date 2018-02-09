# Cross-Origin Read Blocking (CORB)

This document outlines Cross-Origin Read Blocking, an algorithm by which some
dubious cross-origin resource loads may be identified and blocked by web
browsers before they reach the web page. CORB offers a way to maintain same-
origin protections on user data, even in the presence of side channel attacks.


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
      `audioWorkler.addModule()`, etc.
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
is really a *document* by sniffing the response body:

* CORB will only sniff to confirm the classification based on the `Content-Type`
  header (i.e. if the `Content-Type` header is `text/json` then CORB will sniff
  for JSON and will not sniff for HTML and/or XML).

* CORB will only sniff to confirm if the response body is a HTML, XML or JSON
  *document* (and won't attempt to sniff content of other types of *documents*
  like PDFs and/or ZIP archives).

> [lukasza@chromium.org] It is not practical to try teaching CORB about sniffing
> all possible types of *documents* like `application/pdf`, `application/zip`,
> etc.

> [lukasza@chromium.org] Some *document* types are inherently not sniffable
> (for example `text/plain`).

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

Sniffing is a best-effort heuristic and for best security results, we recommend
1) marking responses with the correct Content-Type header and 2) opting out of
sniffing by using the `X-Content-Type-Options: nosniff` header.


### What is CORB compatibility with existing websites?

CORB has no impact on the following scenarios:

* **Prefetch**
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

If CORB can reliably distinguish *documents* from *resources*, then CORB should
be mostly non-disruptive and effectively transparent to websites.  For example,
`<img src="https://example.com/document.html">` should behave the same when
given an HTML document without CORB and when given an empty response body in
presence of CORB.  Still, there are some cases where CORB can cause observable
changes in web behavior, even when sniffing identifies the response as requiring
protection:

* **HTML response which is also a valid CSS**.
  Some responses may sniff as valid HTML, but might also parse as valid CSS.
  When such responses get blocked by CORB, it may break websites using them
  as stylesheets.  Example of such a polyglot response:
```css
<!DOCTYPE html>
<style> h2 {}
h1 { color: blue; }
</style>
```

* **Javascript syntax errors**.
  HTML document loaded into a `<script>` element will typically result in syntax
  errors which can be observed via
  [GlobalEventHandlers.onerror](https://developer.mozilla.org/en-US/docs/Web/API/GlobalEventHandlers/onerror)
  event handler.  Since CORB replaces body of HTML documents with an empty body,
  no syntax error will be reported when CORB is present.

> [lukasza@chromium.org] Maybe CORB should instead replace the response body
> with something like "Blocked by CORB" (which would still trigger a syntax
> error).  Not sure if it is really worth 1) the effort and 2) inconsistency
> with filtered/opaque response from Fetch spec.

CORB can mistakenly treat a *resource* as if it were a *document* when **both**
of the following conditions take place:

* The Content-Type header incorrectly indicates that the response is a
  *document* because either
  - An incorrect Content-Type header is present and is not one of explicitly
    whitelisted types (not an image/*, application/javascript, multipart/*,
    etc.).
  - The Content-Type header is missing

* CORB is not able to confirm that the Content-Type matches response body, when
  either:
  - `X-Content-Type-Options: nosniff` header is present.
  - The Content-Type is not one of types sniffable by CORB (HTML, JSON or XML).
  - The response is a 206 content range responses with a single-range.

> [lukasza@chromium.org] We believe that mislabeling as HTML, JSON or XML is
> most common.  TODO: are we able to back this up with some numbers?

> [lukasza@chromium.org] Note that range requests are typically not issued
> when making requests for scripts and/or stylesheets.

When CORB mistakenly treats a *resource* as if it were a *document*, then it may
have the following effect observable either by scripts or users (possibly
breaking the website that depends on the mislabeled *resources*):

* Rendering of blocked images, videos, audio may be broken
* Scripts may not execute
* Stylesheets may not apply

> [lukasza@chromium.org] TODO: Try to quantify how many websites might be
> impacted.  This discussion will probably have to take place (or at least
> start) in a Chrome-internal document - AFAIK UMA results should not be
> shared publicly :-/

Note that CORB is inactive in the following scenarios:

* Requests allowed via CORS
* Requests initiated by content scripts or plugins

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
> one of explainer sections above?

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
* https://fetch.spec.whatwg.org/#should-response-to-request-be-blocked-due-to-nosniff
* https://fetch.spec.whatwg.org/#x-content-type-options-header
* https://tools.ietf.org/html/rfc7233#section-4 (Responses to a Range Request)
