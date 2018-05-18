// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/process/process_handle.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/service_manager.mojom.h"

namespace content {

class CONTENT_EXPORT AudioServiceListener
    : public service_manager::mojom::ServiceManagerListener {
 public:
  explicit AudioServiceListener(
      std::unique_ptr<service_manager::Connector> connector);
  ~AudioServiceListener() override;

  base::ProcessId GetProcessId() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithoutAudioService_ProcessIdNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnInitWithAudioService_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnAudioServiceCreated_ProcessIdNotNull);
  FRIEND_TEST_ALL_PREFIXES(AudioServiceListenerTest,
                           OnAudioServiceStopped_ProcessIdNull);

  // service_manager::mojom::ServiceManagerListener implementation.
  void OnInit(std::vector<service_manager::mojom::RunningServiceInfoPtr>
                  running_services) override;
  void OnServiceCreated(
      service_manager::mojom::RunningServiceInfoPtr service) override;
  void OnServiceStarted(const ::service_manager::Identity& identity,
                        uint32_t pid) override;
  void OnServicePIDReceived(const ::service_manager::Identity& identity,
                            uint32_t pid) override;
  void OnServiceFailedToStart(
      const ::service_manager::Identity& identity) override;
  void OnServiceStopped(const ::service_manager::Identity& identity) override;

  mojo::Binding<service_manager::mojom::ServiceManagerListener> binding_;
  std::unique_ptr<service_manager::Connector> connector_;

  base::ProcessId process_id_ = base::kNullProcessId;

  SEQUENCE_CHECKER(owning_sequence_);

  DISALLOW_COPY_AND_ASSIGN(AudioServiceListener);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SERVICE_LISTENER_H_
