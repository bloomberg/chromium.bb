// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The PrinterConfigCache class accepts requests to fetch things from
// the Chrome OS Printing serving root. It only stores things in memory.
//
// In practice, the present class fetches either PPDs or PPD metadata.

#ifndef CHROMEOS_PRINTING_PRINTER_CONFIG_CACHE_H_
#define CHROMEOS_PRINTING_PRINTER_CONFIG_CACHE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_piece.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace chromeos {

// A PrinterConfigCache maps keys to values. By convention, keys are
// relative paths to files in the Chrome OS Printing serving root
// (hardcoded into this class). In practice, that means keys will either
// *  start with "metadata_v3/" or
// *  start with "ppds_for_metadata_v3/."
//
// This class must always be constructed on, used on, and destroyed from
// a sequenced context.
//
// TODO(crbug.com/888189): remove CHROMEOS_EXPORT and refactor all this
// into a dedicated gn component.
class CHROMEOS_EXPORT PrinterConfigCache {
 public:
  static std::unique_ptr<PrinterConfigCache> Create(
      const base::Clock* clock,
      network::mojom::URLLoaderFactory* loader_factory);
  virtual ~PrinterConfigCache() = default;

  // Result of calling Fetch(). The |key| identifies how Fetch() was
  // originally invoked. The |contents| and |time_of_fetch| are well-
  // defined iff |succeeded| is true.
  struct FetchResult {
    static FetchResult Failure(const std::string& key);

    static FetchResult Success(const std::string& key,
                               const std::string& contents,
                               base::Time time_of_fetch);

    bool succeeded;
    std::string key;
    std::string contents;
    base::Time time_of_fetch;
  };

  // Caller is responsible for providing sequencing of this type.
  using FetchCallback = base::OnceCallback<void(const FetchResult&)>;

  // Queries the Chrome OS Printing serving root for |key|. Calls |cb|
  // with the contents. If an entry newer than |expiration| is resident,
  // calls |cb| immediately with those contents. Caller should not pass
  // keys with leading slashes.
  //
  // Using TimeDelta implies the caller is asking for "some entry not
  // older than |expiration|," e.g. "metadata_v3/index-00.json that
  // was fetched within the last 30 minutes."
  //
  // Naturally,
  // *  passing the Max() TimeDelta means "perform this Fetch() with no
  //    limit on staleness" and
  // *  passing a zero TimeDelta should practically force a networked
  //    fetch (less esoteric timing quirks etc.).
  virtual void Fetch(const std::string& key,
                     base::TimeDelta expiration,
                     FetchCallback cb) = 0;

  // Drops Entry corresponding to |key|.
  virtual void Drop(const std::string& key) = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_PRINTING_PRINTER_CONFIG_CACHE_H_
