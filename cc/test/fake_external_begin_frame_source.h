// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_EXTERNAL_BEGIN_FRAME_SOURCE_H_
#define CC_TEST_FAKE_EXTERNAL_BEGIN_FRAME_SOURCE_H_

#include <set>

#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/begin_frame_source.h"

namespace base {
class SimpleTestTickClock;
}  // namespace base

namespace cc {

class FakeExternalBeginFrameSource
    : public BeginFrameSource,
      public NON_EXPORTED_BASE(base::NonThreadSafe) {
 public:
  class Client {
   public:
    virtual void OnAddObserver(BeginFrameObserver* obs) = 0;
    virtual void OnRemoveObserver(BeginFrameObserver* obs) = 0;
  };

  explicit FakeExternalBeginFrameSource(double refresh_rate,
                                        bool tick_automatically);
  ~FakeExternalBeginFrameSource() override;

  void SetClient(Client* client) { client_ = client; }
  void SetPaused(bool paused);

  // BeginFrameSource implementation.
  void AddObserver(BeginFrameObserver* obs) override;
  void RemoveObserver(BeginFrameObserver* obs) override;
  void DidFinishFrame(BeginFrameObserver* obs,
                      const BeginFrameAck& ack) override;
  bool IsThrottled() const override;

  BeginFrameArgs CreateBeginFrameArgs(
      BeginFrameArgs::CreationLocation location);
  BeginFrameArgs CreateBeginFrameArgs(BeginFrameArgs::CreationLocation location,
                                      base::SimpleTestTickClock* now_src);
  uint64_t next_begin_frame_number() const { return next_begin_frame_number_; }

  void TestOnBeginFrame(const BeginFrameArgs& args);

  size_t num_observers() const { return observers_.size(); }

  const BeginFrameAck& LastAckForObserver(BeginFrameObserver* obs) {
    return last_acks_[obs];
  }

 private:
  void PostTestOnBeginFrame();

  const bool tick_automatically_;
  const double milliseconds_per_frame_;
  Client* client_ = nullptr;
  bool paused_ = false;
  BeginFrameArgs current_args_;
  uint64_t next_begin_frame_number_ = BeginFrameArgs::kStartingFrameNumber;
  std::set<BeginFrameObserver*> observers_;
  base::CancelableCallback<void(const BeginFrameArgs&)> begin_frame_task_;
  std::unordered_map<BeginFrameObserver*, BeginFrameAck> last_acks_;
  base::WeakPtrFactory<FakeExternalBeginFrameSource> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_EXTERNAL_BEGIN_FRAME_SOURCE_H_
