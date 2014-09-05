// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include "base/macros.h"
#include "media/base/buffering_state.h"
#include "media/base/decoder_buffer.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace mojo {

#define ASSERT_ENUM_VALUES_EQUAL(value)                              \
  COMPILE_ASSERT(media::BUFFERING_##value ==                         \
      static_cast<media::BufferingState>(BUFFERING_STATE_##value),   \
          value##_enum_value_matches)

ASSERT_ENUM_VALUES_EQUAL(HAVE_NOTHING);
ASSERT_ENUM_VALUES_EQUAL(HAVE_ENOUGH);

// static
MediaDecoderBufferPtr TypeConverter<MediaDecoderBufferPtr,
    scoped_refptr<media::DecoderBuffer> >::Convert(
        const scoped_refptr<media::DecoderBuffer>& input) {
  MediaDecoderBufferPtr mojo_buffer(MediaDecoderBuffer::New());
  mojo_buffer->timestamp_usec = input->timestamp().InMicroseconds();
  mojo_buffer->duration_usec = input->duration().InMicroseconds();
  mojo_buffer->data_size = input->data_size();
  mojo_buffer->side_data_size = input->side_data_size();
  mojo_buffer->front_discard_usec =
      input->discard_padding().first.InMicroseconds();
  mojo_buffer->back_discard_usec =
      input->discard_padding().second.InMicroseconds();
  mojo_buffer->splice_timestamp_usec =
      input->splice_timestamp().InMicroseconds();

  // TODO(tim): Assuming this is small so allowing extra copies.
  std::vector<uint8> side_data(input->side_data(),
                               input->side_data() + input->side_data_size());
  mojo_buffer->side_data.Swap(&side_data);

  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = input->data_size();
  DataPipe data_pipe(options);
  mojo_buffer->data = data_pipe.consumer_handle.Pass();

  uint32_t num_bytes = input->data_size();
  // TODO(tim): ALL_OR_NONE isn't really appropriate. Check success?
  // If fails, we'd still return the buffer, but we'd need to HandleWatch
  // to fill the pipe at a later time, which means the de-marshalling code
  // needs to wait for a readable pipe (which it currently doesn't).
  WriteDataRaw(data_pipe.producer_handle.get(),
               input->data(),
               &num_bytes,
               MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
  return mojo_buffer.Pass();
}

// static
scoped_refptr<media::DecoderBuffer>  TypeConverter<
    scoped_refptr<media::DecoderBuffer>, MediaDecoderBufferPtr>::Convert(
        const MediaDecoderBufferPtr& input) {
  uint32_t num_bytes  = 0;
  // TODO(tim): We're assuming that because we always write to the pipe above
  // before sending the MediaDecoderBuffer that the pipe is readable when
  // we get here.
  ReadDataRaw(input->data.get(), NULL, &num_bytes, MOJO_READ_DATA_FLAG_QUERY);
  CHECK_EQ(num_bytes, input->data_size) << "Pipe error converting buffer";

  scoped_ptr<uint8[]> data(new uint8[num_bytes]);  // Uninitialized.
  ReadDataRaw(input->data.get(), data.get(), &num_bytes,
              MOJO_READ_DATA_FLAG_ALL_OR_NONE);
  CHECK_EQ(num_bytes, input->data_size) << "Pipe error converting buffer";

  // TODO(tim): We can't create a media::DecoderBuffer that has side_data
  // without copying data because it wants to ensure alignment. Could we
  // read directly into a pre-padded DecoderBuffer?
  scoped_refptr<media::DecoderBuffer> buffer;
  if (input->side_data_size) {
    buffer = media::DecoderBuffer::CopyFrom(data.get(),
                                            num_bytes,
                                            input->side_data.storage().data(),
                                            input->side_data_size);
  } else {
    buffer = media::DecoderBuffer::CopyFrom(data.get(), num_bytes);
  }

  buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(input->timestamp_usec));
  buffer->set_duration(
      base::TimeDelta::FromMicroseconds(input->duration_usec));
  media::DecoderBuffer::DiscardPadding discard_padding(
      base::TimeDelta::FromMicroseconds(input->front_discard_usec),
      base::TimeDelta::FromMicroseconds(input->back_discard_usec));
  buffer->set_discard_padding(discard_padding);
  buffer->set_splice_timestamp(
      base::TimeDelta::FromMicroseconds(input->splice_timestamp_usec));
  return buffer;
}

}  // namespace mojo
