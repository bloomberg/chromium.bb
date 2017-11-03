// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/memlog_receiver.h"
#include "chrome/profiling/memlog_stream_parser.h"

#include <utility>

namespace profiling {
namespace {

class DummyMemlogReceiver : public profiling::MemlogReceiver {
  void OnHeader(const StreamHeader& header) override {}
  void OnAlloc(const AllocPacket& alloc_packet,
               std::vector<Address>&& stack,
               std::string&& context) override {}
  void OnFree(const FreePacket& free_packet) override {}
  void OnBarrier(const BarrierPacket& barrier_packet) override {}
  void OnComplete() override {}
};

}  // namespace
}  // namespace profiling

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  profiling::DummyMemlogReceiver receiver;
  scoped_refptr<profiling::MemlogStreamParser> parser(
      new profiling::MemlogStreamParser(&receiver));
  std::unique_ptr<char[]> stream_data(new char[size]);
  memcpy(stream_data.get(), data, size);
  parser->OnStreamData(std::move(stream_data), size);
  return 0;
}
