// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SDCH_SDCH_OWNER_H_
#define NET_SDCH_SDCH_OWNER_H_

#include <string>

#include "base/memory/memory_pressure_listener.h"
#include "net/base/sdch_observer.h"
#include "net/url_request/sdch_dictionary_fetcher.h"

class GURL;

namespace base {
class Clock;
}

namespace net {
class SdchManager;
class URLRequestContext;

// This class owns the SDCH objects not owned as part of URLRequestContext, and
// exposes interface for setting SDCH policy.  It should be instantiated by
// the net/ embedder.
// TODO(rdsmith): Implement dictionary prioritization.
class NET_EXPORT SdchOwner : public net::SdchObserver {
 public:
  static const size_t kMaxTotalDictionarySize;
  static const size_t kMinSpaceForDictionaryFetch;

  // Consumer must guarantee that |sdch_manager| and |context| outlive
  // this object.
  SdchOwner(net::SdchManager* sdch_manager, net::URLRequestContext* context);
  ~SdchOwner() override;

  // Defaults to kMaxTotalDictionarySize.
  void SetMaxTotalDictionarySize(size_t max_total_dictionary_size);

  // Defaults to kMinSpaceForDictionaryFetch.
  void SetMinSpaceForDictionaryFetch(size_t min_space_for_dictionary_fetch);

  // SdchObserver implementation.
  void OnDictionaryUsed(SdchManager* manager,
                        const std::string& server_hash) override;
  void OnGetDictionary(net::SdchManager* manager,
                       const GURL& request_url,
                       const GURL& dictionary_url) override;
  void OnClearDictionaries(net::SdchManager* manager) override;

  // Implementation detail--this is the pathway through which the
  // fetcher informs the SdchOwner that it's gotten the dictionary.
  // Public for testing.
  void OnDictionaryFetched(const std::string& dictionary_text,
                           const GURL& dictionary_url,
                           const net::BoundNetLog& net_log);

  void SetClockForTesting(scoped_ptr<base::Clock> clock);

 private:
  // For each active dictionary, stores local info.
  // Indexed by server hash.
  struct DictionaryInfo {
    base::Time last_used;
    int use_count;
    size_t size;

    DictionaryInfo() : use_count(0), size(0) {}
    DictionaryInfo(const base::Time& last_used, size_t size)
        : last_used(last_used), use_count(0), size(size) {}
    DictionaryInfo(const DictionaryInfo& rhs) = default;
    DictionaryInfo& operator=(const DictionaryInfo& rhs) = default;
  };

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  net::SdchManager* manager_;
  net::SdchDictionaryFetcher fetcher_;

  std::map<std::string, DictionaryInfo> local_dictionary_info_;
  size_t total_dictionary_bytes_;

  scoped_ptr<base::Clock> clock_;

  size_t max_total_dictionary_size_;
  size_t min_space_for_dictionary_fetch_;

#if defined(OS_CHROMEOS)
  // For debugging http://crbug.com/454198; remove when resolved.
  unsigned int destroyed_;
#endif

  // TODO(rmcilroy) Add back memory_pressure_listener_ when
  // http://crbug.com/447208 is fixed

  DISALLOW_COPY_AND_ASSIGN(SdchOwner);
};

}  // namespace net

#endif  // NET_SDCH_SDCH_OWNER_H_
