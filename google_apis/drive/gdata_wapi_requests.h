// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_GDATA_WAPI_REQUESTS_H_
#define GOOGLE_APIS_DRIVE_GDATA_WAPI_REQUESTS_H_

#include <string>

#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"

namespace google_apis {

class ResourceEntry;

// Callback type for GetResourceEntryRequest.
typedef base::Callback<void(GDataErrorCode error,
                            scoped_ptr<ResourceEntry> entry)>
    GetResourceEntryCallback;

// This class performs the request for fetching a single resource entry.
class GetResourceEntryRequest : public UrlFetchRequestBase {
 public:
  // |callback| must not be null.
  GetResourceEntryRequest(RequestSender* sender,
                          const GDataWapiUrlGenerator& url_generator,
                          const std::string& resource_id,
                          const GURL& embed_origin,
                          const GetResourceEntryCallback& callback);
  virtual ~GetResourceEntryRequest();

 protected:
  // UrlFetchRequestBase overrides.
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode error) OVERRIDE;
  virtual GURL GetURL() const OVERRIDE;

 private:
  void OnDataParsed(GDataErrorCode error, scoped_ptr<ResourceEntry> entry);

  const GDataWapiUrlGenerator url_generator_;
  // Resource id of the requested entry.
  const std::string resource_id_;
  // Embed origin for an url to the sharing dialog. Can be empty.
  GURL embed_origin_;

  const GetResourceEntryCallback callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<GetResourceEntryRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetResourceEntryRequest);
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_WAPI_REQUESTS_H_
