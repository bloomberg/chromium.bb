// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/simple_event_subscriber.h"

#include <vector>

#include "base/logging.h"
#include "base/single_thread_task_runner.h"

namespace media {
namespace cast {

SimpleEventSubscriber::SimpleEventSubscriber(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_proxy) {
  DCHECK(main_thread_proxy->RunsTasksOnCurrentThread());
}

SimpleEventSubscriber::~SimpleEventSubscriber() {
  thread_checker_.CalledOnValidThread();
}

void SimpleEventSubscriber::OnReceiveFrameEvent(const FrameEvent& frame_event) {
  thread_checker_.CalledOnValidThread();
  frame_events_.push_back(frame_event);
}

void SimpleEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  thread_checker_.CalledOnValidThread();
  packet_events_.push_back(packet_event);
}

void SimpleEventSubscriber::OnReceiveGenericEvent(
    const GenericEvent& generic_event) {
  generic_events_.push_back(generic_event);
}

void SimpleEventSubscriber::GetFrameEventsAndReset(
    std::vector<FrameEvent>* frame_events) {
  thread_checker_.CalledOnValidThread();
  frame_events->swap(frame_events_);
  frame_events_.clear();
}

void SimpleEventSubscriber::GetPacketEventsAndReset(
    std::vector<PacketEvent>* packet_events) {
  thread_checker_.CalledOnValidThread();
  packet_events->swap(packet_events_);
  packet_events_.clear();
}

void SimpleEventSubscriber::GetGenericEventsAndReset(
    std::vector<GenericEvent>* generic_events) {
  thread_checker_.CalledOnValidThread();
  generic_events->swap(generic_events_);
  generic_events_.clear();
}

}  // namespace cast
}  // namespace media
