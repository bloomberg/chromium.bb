# Service workers
[content/browser/service_worker]: /content/browser/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/common/service_worker]: /content/common/service_worker
[disk_cache]: /net/disk_cache/README.md
[embedded_worker.mojom]: https://codesearch.chromium.org/chromium/src/content/common/service_worker/embedded_worker.mojom
[service_worker_container.mojom]: https://codesearch.chromium.org/chromium/src/third_party/blink/public/mojom/service_worker/service_worker_container.mojom
[service_worker_database.h]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_database.h
[third_party/blink/common/service_worker]: /third_party/blink/common/service_worker
[third_party/blink/public/common/service_worker]: /third_party/blink/public/common/service_worker
[third_party/blink/public/mojom/service_worker]: /third_party/blink/public/mojom/service_worker
[third_party/blink/public/platform/modules/service_worker]: /third_party/blink/public/platform/modules/service_worker
[third_party/blink/public/web/modules/service_worker]: /third_party/blink/public/web/modules/service_worker
[third_party/blink/renderer/modules/service_worker]: /third_party/blink/renderer/modules/service_worker
[Blink Public API]: /third_party/blink/public/README.md
[Cache Storage API]: /content/browser/cache_storage/README.md
[LevelDB]: /third_party/leveldatabase/README.chromium
[Onion Soup]: https://docs.google.com/document/d/1K1nO8G9dO9kNSmtVz2gJ2GG9gQOTgm65sJlV3Fga4jE/edit?usp=sharing
[Quota Manager]: /storage/browser/quota
[ServiceWorkerDatabase]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_database.h
[ServiceWorkerStorage]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_storage.h
[Service Worker specification]: https://w3c.github.io/ServiceWorker/

This is Chromium's implementation of [service
workers](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API).
See the [Service Worker specification].

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

## Storage

Service worker storage consists of the following.
- **Service worker registration metadata** is stored in a [LevelDB] instance
  located at ${DIR_USER_DATA}/Service Worker/Database.
- **Service worker scripts** are stored in a [disk_cache] instance using the
  "simple" implementation, located at ${DIR_USER_DATA}/Service
  Worker/ScriptCache. Registration metadata points to these scripts.

Code pointers include [ServiceWorkerDatabase] and [ServiceWorkerStorage].

The related [Cache Storage API] uses a [disk_cache] instance using the "simple"
implementation, located at ${DIR_USER_DATA}/Service Worker/CacheStorage. This
location was chosen because the [Cache Storage API] is currently defined in the
[Service Worker specification], but it can be used independently of service
workers.

For incognito windows, everything is in-memory.

### Eviction

Service workers storage lasts indefinitely, i.e, there is no periodic deletion
of old but still installed service workers. Installed service workers are only
evicted by the [Quota Manager] (or user action). The Quota Manager controls
several web platform APIs, including sandboxed filesystem, WebSQL, appcache,
IndexedDB, cache storage, service worker (registration and scripts), and
background fetch.

The Quota Manager starts eviction when one of the following conditions is true
(as of August 2018):
- **The global pool is full**: Chrome is using > 1/3 of the disk (>2/3 on CrOS).
- **The system is critically low on space**: the disk has < min(1GB,1%) free
  (regardless of how much Chrome is contributing!)

When eviction starts, origins are purged on an LRU basis until the triggering
condition no longer applies. Purging an origin deletes its storage completely.

Note that Quota Manager eviction is independent of HTTP cache eviction. The
HTTP cache is typically much smaller than the storage under the control of the
Quota Manager, and it likely uses a simple non-origin-based LRU algorithm.

## UseCounter integration

Blink has a UseCounter mechanism intended to measure the percentage of page
loads on the web that used a given feature.  Service workers complicate this
measurement because a feature use in a service worker potentially affects many
page loads, including ones in the future.

Therefore, service workers integrate with the UseCounter mechanism as follows:
- **If a feature use occurs before the service worker finished installing**, it
is recorded in storage along with the service worker. Any page thereafter that
the service worker controls is counted as using the feature.
- **If a feature use occurs after the service worker finished installing**, all
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
