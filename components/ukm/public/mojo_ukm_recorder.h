// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_PUBLIC_MOJO_UKM_RECORDER_H_
#define COMPONENTS_UKM_PUBLIC_MOJO_UKM_RECORDER_H_

#include "components/ukm/public/interfaces/ukm_interface.mojom.h"
#include "components/ukm/public/ukm_recorder.h"

namespace ukm {

/**
 * A helper wrapper that lets UKM data be recorded on other processes with the
 * same interface that is used in the browser process.
 *
 * Usage Example:
 *
 *  ukm::mojom::UkmRecorderInterfacePtr interface;
 *  content::RenderThread::Get()->GetConnector()->BindInterface(
 *      content::mojom::kBrowserServiceName, mojo::MakeRequest(&interface));
 *  ukm::MojoUkmRecorder recorder(std::move(interface));
 *  std::unique_ptr<ukm::UkmEntryBuilder> builder =
 *      recorder.GetEntryBuilder(coordination_unit_id, "MyEvent");
 *  builder->AddMetric("MyMetric", metric_value);
 */
class MojoUkmRecorder : public UkmRecorder {
 public:
  MojoUkmRecorder(mojom::UkmRecorderInterfacePtr interface);
  ~MojoUkmRecorder() override;

 private:
  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void AddEntry(mojom::UkmEntryPtr entry) override;

  mojom::UkmRecorderInterfacePtr interface_;

  DISALLOW_COPY_AND_ASSIGN(MojoUkmRecorder);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_PUBLIC_MOJO_UKM_RECORDER_H_
