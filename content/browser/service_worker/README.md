# Service workers
[content/browser/service_worker]: /content/browser/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/common/service_worker]: /content/common/service_worker
[embedded_worker.mojom]: https://codesearch.chromium.org/chromium/src/content/common/service_worker/embedded_worker.mojom
[service_worker_container.mojom]: https://codesearch.chromium.org/chromium/src/content/common/service_worker/service_worker_container.mojom
[service_worker_database.h]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_database.h
[third_party/blink/common/service_worker]: /third_party/blink/common/service_worker
[third_party/blink/public/common/service_worker]: /third_party/blink/public/common/service_worker
[third_party/blink/public/mojom/service_worker]: /third_party/blink/public/mojom/service_worker
[third_party/blink/public/platform/modules/service_worker]: /third_party/blink/public/platform/modules/service_worker
[third_party/blink/public/web/modules/service_worker]: /third_party/blink/public/web/modules/service_worker
[third_party/blink/renderer/modules/service_worker]: /third_party/blink/renderer/modules/service_worker
[Blink Public API]: /third_party/blink/public
[Onion Soup]: https://docs.google.com/document/d/1K1nO8G9dO9kNSmtVz2gJ2GG9gQOTgm65sJlV3Fga4jE/edit?usp=sharing

This is Chromium's implementation of [service
workers](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API). See the
[Service Worker specification](https://w3c.github.io/ServiceWorker/).

## Directory structure

- [content/browser/service_worker]: Browser process code, including stored
  registration data, the inception of starting a service worker, and controlling
  navigations. The browser process has host objects of most live renderer
  entities that deal with service workers, and the bulk of work is performed by
  these host objects.
- [content/renderer/service_worker]: Renderer process code. This should move to
  third_party/blink per [Onion Soup].
- [content/common/service_worker]: Common process code.
- [third_party/blink/common/service_worker]: Common process code. Contains the
  implementation of [third_party/blink/public/common/service_worker].
- [third_party/blink/public/common/service_worker]: Header files for common
  process code that can be used by both inside Blink and outside Blink.
- [third_party/blink/public/mojom/service_worker]: Mojom files for common
  process code that can be used by both Blink and content.
- [third_party/blink/public/platform/modules/service_worker]: [Blink Public API]
  header files. This should be removed per [Onion Soup].
- [third_party/blink/public/web/modules/service_worker]: More [Blink Public API]
  header files. This should be removed per [Onion Soup].
- [third_party/blink/renderer/modules/service_worker]: Renderer process code in
  Blink. This is the closest code to the web-exposed Service Worker API.

## UseCounter integration

Blink has a UseCounter mechanism intended to measure the percentage of page
loads on the web that used a given feature.  Service workers complicate this
measurement because a feature use in a service worker potentially affects many
page loads, including ones in the future.

Therefore, service workers integrate with the UseCounter mechanism as follows:
- If a feature use occurs before the service worker finished installing, it
is recorded in storage along with the service worker. Any page thereafter that
the service worker controls is counted as using the feature.
- If a feature use occurs after the service worker finished installing, all
currently controlled pages are counted as using the feature.

For more details and rationale, see [Design of UseCounter for
workers](https://docs.google.com/document/d/1VyYZnhjBdk-MzCRAcX37TM5-yjwTY40U_J9rWnEAo8c/edit?usp=sharing)
and [crbug 376039](https://bugs.chromium.org/p/chromium/issues/detail?id=376039).

Code pointers include:
- (Browser -> Page) ServiceWorkerContainer.SetController and
ServiceWorkerContainer.CountFeature in [service_worker_container.mojom].
- (Service worker -> Browser) EmbeddedWorkerInstanceHost.CountFeature
in [embedded_worker.mojom].
- (Persistence) ServiceWorkerDatabase::RegistrationData::used_features
in [service_worker_database.h].

## Other documentation

- [Service Worker Security FAQ](/docs/security/service-worker-security-faq.md)
