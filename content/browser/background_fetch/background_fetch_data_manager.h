// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/browser/background_fetch/background_fetch_registration_id.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"
#include "url/origin.h"

namespace content {

class BackgroundFetchRequestInfo;
struct BackgroundFetchSettledFetch;
class BlobHandle;
class BrowserContext;
class ChromeBlobStorageContext;

// The BackgroundFetchDataManager keeps track of all of the outstanding requests
// which are in process in the DownloadManager. When Chromium restarts, it is
// responsibile for reconnecting all the in progress downloads with an observer
// which will keep the metadata up to date.
class CONTENT_EXPORT BackgroundFetchDataManager {
 public:
  using CreateRegistrationCallback = base::OnceCallback<void(
      blink::mojom::BackgroundFetchError,
      std::vector<scoped_refptr<BackgroundFetchRequestInfo>>)>;
  using DeleteRegistrationCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError)>;
  using NextRequestCallback =
      base::OnceCallback<void(scoped_refptr<BackgroundFetchRequestInfo>)>;
  using SettledFetchesCallback =
      base::OnceCallback<void(blink::mojom::BackgroundFetchError,
                              bool /* background_fetch_succeeded */,
                              std::vector<BackgroundFetchSettledFetch>,
                              std::vector<std::unique_ptr<BlobHandle>>)>;

  explicit BackgroundFetchDataManager(BrowserContext* browser_context);
  ~BackgroundFetchDataManager();

  // Creates and stores a new registration with the given properties. Will
  // invoke the |callback| when the registration has been created, which may
  // fail due to invalid input or storage errors.
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      CreateRegistrationCallback callback);

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has been started as |download_guid|.
  void MarkRequestAsStarted(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request,
      const std::string& download_guid);

  // Marks that the |request|, part of the Background Fetch identified by
  // |registration_id|, has completed. Will invoke the |callback| with the
  // next request, if any, when the operation has completed.
  void MarkRequestAsCompleteAndGetNextRequest(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request,
      NextRequestCallback callback);

  // Reads all settled fetches for the given |registration_id|. Both the Request
  // and Response objects will be initialised based on the stored data. Will
  // invoke the |callback| when the list of fetches has been compiled.
  void GetSettledFetchesForRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      SettledFetchesCallback callback);

  // Deletes the registration identified by |registration_id|. Will invoke the
  // |callback| when the registration has been deleted from storage.
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          DeleteRegistrationCallback callback);

 private:
  friend class BackgroundFetchDataManagerTest;

  class RegistrationData;

  // The blob storage request with which response information will be stored.
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Map of known background fetch registration ids to their associated data.
  std::map<BackgroundFetchRegistrationId, std::unique_ptr<RegistrationData>>
      registrations_;

  base::WeakPtrFactory<BackgroundFetchDataManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchDataManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_DATA_MANAGER_H_
