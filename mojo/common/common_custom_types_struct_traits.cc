// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_custom_types_struct_traits.h"

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
bool StructTraits<common::mojom::String16DataView, base::string16>::Read(
    common::mojom::String16DataView data,
    base::string16* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  out->assign(reinterpret_cast<const base::char16*>(view.data()), view.size());
  return true;
}

// static
const std::vector<uint32_t>&
StructTraits<common::mojom::VersionDataView, base::Version>::components(
    const base::Version& version) {
  return version.components();
}

// static
bool StructTraits<common::mojom::VersionDataView, base::Version>::Read(
    common::mojom::VersionDataView data,
    base::Version* out) {
  std::vector<uint32_t> components;
  if (!data.ReadComponents(&components))
    return false;

  *out = base::Version(base::Version(std::move(components)));
  return out->IsValid();
}

// static
bool StructTraits<
    common::mojom::UnguessableTokenDataView,
    base::UnguessableToken>::Read(common::mojom::UnguessableTokenDataView data,
                                  base::UnguessableToken* out) {
  uint64_t high = data.high();
  uint64_t low = data.low();

  // Receiving a zeroed UnguessableToken is a security issue.
  if (high == 0 && low == 0)
    return false;

  *out = base::UnguessableToken::Deserialize(high, low);
  return true;
}

mojo::ScopedHandle StructTraits<common::mojom::FileDataView, base::File>::fd(
    base::File& file) {
  DCHECK(file.IsValid());
  return mojo::WrapPlatformFile(file.TakePlatformFile());
}

bool StructTraits<common::mojom::FileDataView, base::File>::Read(
    common::mojom::FileDataView data,
    base::File* file) {
  base::PlatformFile platform_handle = base::kInvalidPlatformFile;
  if (mojo::UnwrapPlatformFile(data.TakeFd(), &platform_handle) !=
      MOJO_RESULT_OK) {
    return false;
  }
  *file = base::File(platform_handle);
  return true;
}

// static
common::mojom::TextDirection
EnumTraits<common::mojom::TextDirection, base::i18n::TextDirection>::ToMojom(
    base::i18n::TextDirection text_direction) {
  switch (text_direction) {
    case base::i18n::UNKNOWN_DIRECTION:
      return common::mojom::TextDirection::UNKNOWN_DIRECTION;
    case base::i18n::RIGHT_TO_LEFT:
      return common::mojom::TextDirection::RIGHT_TO_LEFT;
    case base::i18n::LEFT_TO_RIGHT:
      return common::mojom::TextDirection::LEFT_TO_RIGHT;
  }
  NOTREACHED();
  return common::mojom::TextDirection::UNKNOWN_DIRECTION;
}

// static
bool EnumTraits<common::mojom::TextDirection, base::i18n::TextDirection>::
    FromMojom(common::mojom::TextDirection input,
              base::i18n::TextDirection* out) {
  switch (input) {
    case common::mojom::TextDirection::UNKNOWN_DIRECTION:
      *out = base::i18n::UNKNOWN_DIRECTION;
      return true;
    case common::mojom::TextDirection::RIGHT_TO_LEFT:
      *out = base::i18n::RIGHT_TO_LEFT;
      return true;
    case common::mojom::TextDirection::LEFT_TO_RIGHT:
      *out = base::i18n::LEFT_TO_RIGHT;
      return true;
  }
  return false;
}

// static
common::mojom::ThreadPriority
EnumTraits<common::mojom::ThreadPriority, base::ThreadPriority>::ToMojom(
    base::ThreadPriority thread_priority) {
  switch (thread_priority) {
    case base::ThreadPriority::BACKGROUND:
      return common::mojom::ThreadPriority::BACKGROUND;
    case base::ThreadPriority::NORMAL:
      return common::mojom::ThreadPriority::NORMAL;
    case base::ThreadPriority::DISPLAY:
      return common::mojom::ThreadPriority::DISPLAY;
    case base::ThreadPriority::REALTIME_AUDIO:
      return common::mojom::ThreadPriority::REALTIME_AUDIO;
  }
  NOTREACHED();
  return common::mojom::ThreadPriority::BACKGROUND;
}

// static
bool EnumTraits<common::mojom::ThreadPriority, base::ThreadPriority>::FromMojom(
    common::mojom::ThreadPriority input,
    base::ThreadPriority* out) {
  switch (input) {
    case common::mojom::ThreadPriority::BACKGROUND:
      *out = base::ThreadPriority::BACKGROUND;
      return true;
    case common::mojom::ThreadPriority::NORMAL:
      *out = base::ThreadPriority::NORMAL;
      return true;
    case common::mojom::ThreadPriority::DISPLAY:
      *out = base::ThreadPriority::DISPLAY;
      return true;
    case common::mojom::ThreadPriority::REALTIME_AUDIO:
      *out = base::ThreadPriority::REALTIME_AUDIO;
      return true;
  }
  return false;
}

// static
common::mojom::FileError
EnumTraits<common::mojom::FileError, base::File::Error>::ToMojom(
    base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return common::mojom::FileError::kOK;
    case base::File::FILE_ERROR_FAILED:
      return common::mojom::FileError::kFailed;
    case base::File::FILE_ERROR_IN_USE:
      return common::mojom::FileError::kInUse;
    case base::File::FILE_ERROR_EXISTS:
      return common::mojom::FileError::kExists;
    case base::File::FILE_ERROR_NOT_FOUND:
      return common::mojom::FileError::kNotFound;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return common::mojom::FileError::kAccessDenied;
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return common::mojom::FileError::kTooManyOpened;
    case base::File::FILE_ERROR_NO_MEMORY:
      return common::mojom::FileError::kNoMemory;
    case base::File::FILE_ERROR_NO_SPACE:
      return common::mojom::FileError::kNoSpace;
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
      return common::mojom::FileError::kNotADirectory;
    case base::File::FILE_ERROR_INVALID_OPERATION:
      return common::mojom::FileError::kInvalidOperation;
    case base::File::FILE_ERROR_SECURITY:
      return common::mojom::FileError::kSecurity;
    case base::File::FILE_ERROR_ABORT:
      return common::mojom::FileError::kAbort;
    case base::File::FILE_ERROR_NOT_A_FILE:
      return common::mojom::FileError::kNotAFile;
    case base::File::FILE_ERROR_NOT_EMPTY:
      return common::mojom::FileError::kNotEmpty;
    case base::File::FILE_ERROR_INVALID_URL:
      return common::mojom::FileError::kInvalidURL;
    case base::File::FILE_ERROR_IO:
      return common::mojom::FileError::kIO;
    case base::File::FILE_ERROR_MAX:
      NOTREACHED();
      return common::mojom::FileError::kFailed;
  }

  NOTREACHED();
  return common::mojom::FileError::kFailed;
}

// static
bool EnumTraits<common::mojom::FileError, base::File::Error>::FromMojom(
    common::mojom::FileError input,
    base::File::Error* out) {
  switch (input) {
    case common::mojom::FileError::kOK:
      *out = base::File::FILE_OK;
      return true;
    case common::mojom::FileError::kFailed:
      *out = base::File::FILE_ERROR_FAILED;
      return true;
    case common::mojom::FileError::kInUse:
      *out = base::File::FILE_ERROR_IN_USE;
      return true;
    case common::mojom::FileError::kExists:
      *out = base::File::FILE_ERROR_EXISTS;
      return true;
    case common::mojom::FileError::kNotFound:
      *out = base::File::FILE_ERROR_NOT_FOUND;
      return true;
    case common::mojom::FileError::kAccessDenied:
      *out = base::File::FILE_ERROR_ACCESS_DENIED;
      return true;
    case common::mojom::FileError::kTooManyOpened:
      *out = base::File::FILE_ERROR_TOO_MANY_OPENED;
      return true;
    case common::mojom::FileError::kNoMemory:
      *out = base::File::FILE_ERROR_NO_MEMORY;
      return true;
    case common::mojom::FileError::kNoSpace:
      *out = base::File::FILE_ERROR_NO_SPACE;
      return true;
    case common::mojom::FileError::kNotADirectory:
      *out = base::File::FILE_ERROR_NOT_A_DIRECTORY;
      return true;
    case common::mojom::FileError::kInvalidOperation:
      *out = base::File::FILE_ERROR_INVALID_OPERATION;
      return true;
    case common::mojom::FileError::kSecurity:
      *out = base::File::FILE_ERROR_SECURITY;
      return true;
    case common::mojom::FileError::kAbort:
      *out = base::File::FILE_ERROR_ABORT;
      return true;
    case common::mojom::FileError::kNotAFile:
      *out = base::File::FILE_ERROR_NOT_A_FILE;
      return true;
    case common::mojom::FileError::kNotEmpty:
      *out = base::File::FILE_ERROR_NOT_EMPTY;
      return true;
    case common::mojom::FileError::kInvalidURL:
      *out = base::File::FILE_ERROR_INVALID_URL;
      return true;
    case common::mojom::FileError::kIO:
      *out = base::File::FILE_ERROR_IO;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<common::mojom::MemoryAllocatorDumpCrossProcessUidDataView,
                  base::trace_event::MemoryAllocatorDumpGuid>::
    Read(common::mojom::MemoryAllocatorDumpCrossProcessUidDataView data,
         base::trace_event::MemoryAllocatorDumpGuid* out) {
  // Receiving a zeroed MemoryAllocatorDumpCrossProcessUid is a bug.
  if (data.value() == 0)
    return false;

  *out = base::trace_event::MemoryAllocatorDumpGuid(data.value());
  return true;
}

}  // namespace mojo
