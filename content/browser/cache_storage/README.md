# Architecture (as of July 29th 2016)
This document descibes the browser-process implementation of the [Cache
Storage specification](
https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html).

## Major Classes and Ownership
### Ownership
Where '=>' represents ownership, '->' is a reference, and '~>' is a weak
reference.

##### `CacheStorageContextImpl`=>`CacheStorageManager`=>`CacheStorage`=>`CacheStorageCache`
* A `CacheStorageManager` can own multiple `CacheStorage` objects.
* A `CacheStorage` can own multiple `CacheStorageCache` objects.

##### `StoragePartitionImpl`->`CacheStorageContextImpl`
* `StoragePartitionImpl` effectively owns the `CacheStorageContextImpl` in the
  sense that it calls `CacheStorageContextImpl::Shutdown()` on deletion which
  resets its `CacheStorageManager`.

##### `RenderProcessHost`->`CacheStorageDispatcherHost`->`CacheStorageContextImpl`

##### `CacheStorageDispatcherHost`=>`CacheStorageCacheHandle`~>`CacheStorageCache`
* The `CacheStorageDispatcherHost` holds onto handles for:
  * currently running operations
  * JavaScript references to caches
  * recently opened caches (to prevent open/close/open churn)

##### `CacheStorageCacheDataHandle`=>`CacheStorageCacheHandle`~>`CacheStorageCache`
* `CacheStorageCacheDataHandle` is the blob data handle for a response body
  and it holds a `CacheStorageCacheHandle`.  It streams from the
  `disk_cache::Entry` response stream. It's necessary that the
  `disk_cache::Backend` (owned by `CacheStorageCache`) stays open so long as
  one of its `disk_cache::Entry`s is reachable. Otherwise, a new backend might
  open and clobber the entry.

### CacheStorageDispatcherHost
1. Receives IPC messages from a render process and creates the appropriate
   `CacheStorageManager` or `CacheStorageCache` operation.
2. For each operation, holds a `CacheStorageCacheHandle` to keep the cache
   alive since the operation is asynchronous.
3. For each cache reference held by the render process, holds a
   `CacheStorageCacheHandle`.
4. Holds a newly opened cache open for a few seconds (by storing a handle) to
   mitigate rapid opening/closing/opening churn.

### CacheStorageManager
1. Forwards calls to the appropriate `CacheStorage` for a given origin,
   loading `CacheStorage`s on demand.
2. Handles `QuotaManager` and `BrowsingData` calls.

### CacheStorage
1. Manages the caches for a single origin.
2. Handles creation/deletion of caches and updates the index on disk
   accordingly.
3. Manages operations that span multiple caches (e.g., `CacheStorage::Match`).
4. Backend-specific information is handled by `CacheStorage::CacheLoader`

### CacheStorageCache
1. Creates or opens a net::disk_cache (either `SimpleCache` or `MemoryCache`)
   on initialization.
2. Handles add/put/delete/match/keys calls.
3. Owned by `CacheStorage` and deleted either when `CacheStorage` deletes or
   when the last `CacheStorageCacheHandle` for the cache is gone.

### CacheStorageIndex
1. Manages an ordered collection of metadata
   (CacheStorageIndex::CacheStorageMetadata) for each CacheStorageCache owned
   by a given CacheStorage instance.
2. Is serialized by CacheStorage::CacheLoader (WriteIndex/LoadIndex) as a
   Protobuf file.

### CacheStorageCacheHandle
1. Holds a weak reference to a `CacheStorageCache`.
2. When the last `CacheStorageCacheHandle` to a `CacheStorageCache` is
   deleted, so to is the `CacheStorageCache`.
3. The `CacheStorageCache` may be deleted before the `CacheStorageCacheHandle`
   (on `CacheStorage` destruction), so it must be checked for validity before
   use.

## Directory Structure
$PROFILE/Service Worker/CacheStorage/`origin`/`cache`/

Where `origin` is a hash of the origin and `cache` is a GUID generated at the
cache's creation time.

The reason a random directory is used for a cache is so that a cache can be
doomed and still used by old references while another cache with the same name
is created.

### Directory Contents
`CacheStorage` creates its own index file (index.txt), which contains a
mapping of cache names to its path on disk. On `CacheStorage` initialization,
directories not in the index are deleted.

Each `CacheStorageCache` has a `disk_cache::Backend` backend, which writes in
the `CacheStorageCache`'s directory.

## Layout of the disk_cache::Backend
A cache is represented by a `disk_cache::Backend`. The Request/Response pairs
referred to in the specification are stored as `disk_cache::Entry`s.  Each
`disk_cache::Entry` has three streams: one for storing a protobuf with the
request/response metadata (e.g., the headers, the request URL, and opacity
information), another for storing the response body, and a final stream for
storing any additional data (e.g., compiled JavaScript).

The entries are keyed by full URL. This has a few ramifications:
 1. Multiple vary responses for a single request URL are not supported.
 2. Operations that may require scanning multiple URLs (e.g., `ignoreSearch`)
    must scan every entry in the cache.

*The above could be fixed by changes to the backend or by introducing indirect
entries in the cache. The indirect entries would be for the query-stripped
request URL. It would point to entries to each query request/response pair and
for each vary request/response pair.*

## Threads
* CacheStorage classes live on the IO thread. Exceptions include:
  * `CacheStorageContextImpl` which is created on UI but otherwise runs and is
   deleted on IO.
  * `CacheStorageDispatcherHost` which is created on UI but otherwise runs and
    is deleted on IO.
* Index file manipulation and directory creation/deletion occurs on a
  `SequencedTaskRunner` assigned at `CacheStorageContextImpl` creation.
* The `disk_cache::Backend` lives on the IO thread and uses its own worker
  pool to implement async operations.

## Asynchronous Idioms in CacheStorage and CacheStorageCache
1. All async methods should asynchronously run their callbacks.
2. The async methods often include several asynchronous steps. Each step
   passes a continuation callback on to the next. The continuation includes
   all of the necessary state for the operation.
3. Callbacks are guaranteed to run so long as the object
   (`CacheStorageCacheCache` or `CacheStorage`) is still alive. Once the
   object is deleted, the callbacks are dropped. We don't worry about dropped
   callbacks on shutdown. If deleting prior to shutdown, one should `Close()`
   a `CacheStorage` or `CacheStorageCache` to ensure that all operations have
   completed before deleting it.

### Scheduling Operations
Operations are scheduled in a sequential scheduler (`CacheStorageScheduler`).
Each `CacheStorage` and `CacheStorageCache` has its own scheduler. If an
operation freezes, then the scheduler is frozen. If a `CacheStorage` call winds
up calling something from every `CacheStorageCache` (e.g.,
`CacheStorage::Match`), then one frozen `CacheStorageCache` can freeze the
`CacheStorage` as well. This has happened in the past (`Cache::Put` called
`QuotaManager` to determine how much room was available, which in turn called
`Cache::Size`). Be careful to avoid situations in which one operation triggers
a dependency on another operation from the same scheduler.

At the end of an operation, the scheduler needs to be kicked to start the next
operation. The idiom for this in CacheStorage/ is to wrap the operation's
callback with a function that will run the callback as well as advance the
scheduler. So long as the operation runs its wrapped callback the scheduler
will advance.
