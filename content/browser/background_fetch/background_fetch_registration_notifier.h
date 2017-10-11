// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_

#include <stdint.h>
#include <map>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace content {

// Tracks the live BackgroundFetchRegistration objects across the renderer
// processes and provides the functionality to notify them of progress updates.
class CONTENT_EXPORT BackgroundFetchRegistrationNotifier {
 public:
  BackgroundFetchRegistrationNotifier();
  ~BackgroundFetchRegistrationNotifier();

  // Registers the |observer| to be notified when fetches for the registration
  // identified by the |unique_id| progress.
  void AddObserver(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationObserverPtr observer);

  // Notifies any registered observers for the registration identified by the
  // |unique_id| of the progress. This will cause JavaScript events to fire.
  void Notify(const std::string& unique_id,
              uint64_t upload_total,
              uint64_t uploaded,
              uint64_t download_total,
              uint64_t downloaded);

  // Removes the observers for the registration identified by the |unique_id|.
  // When the background fetch was successful, the Notify() function should have
  // been called beforehand to inform developers of the final state.
  void RemoveObservers(const std::string& unique_id);

 private:
  // Called when the connection with the |observer| for the registration
  // identified by the |unique_id| goes away.
  void OnConnectionError(
      const std::string& unique_id,
      blink::mojom::BackgroundFetchRegistrationObserver* observer);

  // Storage of observers keyed by the unique id of a registration.
  std::multimap<std::string,
                blink::mojom::BackgroundFetchRegistrationObserverPtr>
      observers_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchRegistrationNotifier);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_REGISTRATION_NOTIFIER_H_
