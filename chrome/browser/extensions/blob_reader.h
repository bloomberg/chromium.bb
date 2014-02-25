// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BLOB_READER_H_
#define CHROME_BROWSER_EXTENSIONS_BLOB_READER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

class Profile;
namespace net {
class URLFetcher;
}

// This class may only be used from the UI thread.
class BlobReader : public net::URLFetcherDelegate {
 public:
  // |blob_data| contains the portion of the Blob requested. |blob_total_size|
  // is the total size of the Blob, and may be larger than |blob_data->size()|.
  // |blob_total_size| is -1 if it cannot be determined.
  typedef base::Callback<void(scoped_ptr<std::string> blob_data,
                              int64 blob_total_size)>
      BlobReadCallback;

  BlobReader(Profile* profile,
             const std::string& blob_uuid,
             BlobReadCallback callback);
  virtual ~BlobReader();

  void SetByteRange(int64 offset, int64 length);

  void Start();

 private:
  // Overridden from net::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  BlobReadCallback callback_;
  scoped_ptr<net::URLFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(BlobReader);
};

#endif  // CHROME_BROWSER_EXTENSIONS_BLOB_READER_H_
