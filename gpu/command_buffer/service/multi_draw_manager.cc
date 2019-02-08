// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/multi_draw_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/checked_math.h"

namespace gpu {
namespace gles2 {

MultiDrawManager::ResultData::ResultData()
    : draw_function(DrawFunction::None) {}

MultiDrawManager::ResultData::ResultData(ResultData&& rhs)
    : draw_function(rhs.draw_function),
      drawcount(rhs.drawcount),
      mode(rhs.mode),
      type(rhs.type),
      firsts(std::move(rhs.firsts)),
      counts(std::move(rhs.counts)),
      offsets(std::move(rhs.offsets)),
      indices(std::move(rhs.indices)),
      instance_counts(std::move(rhs.instance_counts)) {
  rhs.draw_function = DrawFunction::None;
}

MultiDrawManager::ResultData& MultiDrawManager::ResultData::operator=(
    ResultData&& rhs) {
  if (&rhs == this) {
    return *this;
  }
  draw_function = rhs.draw_function;
  drawcount = rhs.drawcount;
  mode = rhs.mode;
  type = rhs.type;
  std::swap(firsts, rhs.firsts);
  std::swap(counts, rhs.counts);
  std::swap(offsets, rhs.offsets);
  std::swap(indices, rhs.indices);
  std::swap(instance_counts, rhs.instance_counts);

  rhs.draw_function = DrawFunction::None;
  return *this;
}

MultiDrawManager::ResultData::~ResultData() {}

MultiDrawManager::MultiDrawManager(IndexStorageType index_type)
    : current_draw_offset_(0), index_type_(index_type), result_() {}

bool MultiDrawManager::Begin(GLsizei drawcount) {
  result_.drawcount = drawcount;
  current_draw_offset_ = 0;
  if (result_.draw_function != DrawFunction::None) {
    NOTREACHED();
    return false;
  }
  return true;
}

bool MultiDrawManager::End(ResultData* result) {
  DCHECK(result);

  if (result_.draw_function == DrawFunction::None ||
      current_draw_offset_ != result_.drawcount) {
    return false;
  }
  *result = std::move(result_);
  return true;
}

bool MultiDrawManager::MultiDrawArrays(GLenum mode,
                                       const GLint* firsts,
                                       const GLsizei* counts,
                                       GLsizei drawcount) {
  if (!EnsureDrawArraysFunction(DrawFunction::DrawArrays, mode) ||
      base::CheckAdd(current_draw_offset_, drawcount).ValueOrDie() >
          result_.drawcount) {
    NOTREACHED();
    return false;
  }
  std::copy(firsts, firsts + drawcount, &result_.firsts[current_draw_offset_]);
  std::copy(counts, counts + drawcount, &result_.counts[current_draw_offset_]);
  current_draw_offset_ += drawcount;
  return true;
}

bool MultiDrawManager::MultiDrawArraysInstanced(GLenum mode,
                                                const GLint* firsts,
                                                const GLsizei* counts,
                                                const GLsizei* instance_counts,
                                                GLsizei drawcount) {
  if (!EnsureDrawArraysFunction(DrawFunction::DrawArraysInstanced, mode) ||
      base::CheckAdd(current_draw_offset_, drawcount).ValueOrDie() >
          result_.drawcount) {
    NOTREACHED();
    return false;
  }
  std::copy(firsts, firsts + drawcount, &result_.firsts[current_draw_offset_]);
  std::copy(counts, counts + drawcount, &result_.counts[current_draw_offset_]);
  std::copy(instance_counts, instance_counts + drawcount,
            &result_.instance_counts[current_draw_offset_]);
  current_draw_offset_ += drawcount;
  return true;
}

bool MultiDrawManager::MultiDrawElements(GLenum mode,
                                         const GLsizei* counts,
                                         GLenum type,
                                         const GLsizei* offsets,
                                         GLsizei drawcount) {
  if (!EnsureDrawElementsFunction(DrawFunction::DrawElements, mode, type) ||
      base::CheckAdd(current_draw_offset_, drawcount).ValueOrDie() >
          result_.drawcount) {
    NOTREACHED();
    return false;
  }
  std::copy(counts, counts + drawcount, &result_.counts[current_draw_offset_]);
  switch (index_type_) {
    case IndexStorageType::Offset:
      std::copy(offsets, offsets + drawcount,
                &result_.offsets[current_draw_offset_]);
      break;
    case IndexStorageType::Pointer:
      std::transform(
          offsets, offsets + drawcount, &result_.indices[current_draw_offset_],
          [](uint32_t offset) {
            return reinterpret_cast<void*>(static_cast<intptr_t>(offset));
          });
      break;
  }
  current_draw_offset_ += drawcount;
  return true;
}

bool MultiDrawManager::MultiDrawElementsInstanced(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  if (!EnsureDrawElementsFunction(DrawFunction::DrawElementsInstanced, mode,
                                  type) ||
      base::CheckAdd(current_draw_offset_, drawcount).ValueOrDie() >
          result_.drawcount) {
    NOTREACHED();
    return false;
  }
  std::copy(counts, counts + drawcount, &result_.counts[current_draw_offset_]);
  std::copy(instance_counts, instance_counts + drawcount,
            &result_.instance_counts[current_draw_offset_]);
  switch (index_type_) {
    case IndexStorageType::Offset:
      std::copy(offsets, offsets + drawcount,
                &result_.offsets[current_draw_offset_]);
      break;
    case IndexStorageType::Pointer:
      std::transform(
          offsets, offsets + drawcount, &result_.indices[current_draw_offset_],
          [](uint32_t offset) {
            return reinterpret_cast<void*>(static_cast<intptr_t>(offset));
          });
      break;
  }
  current_draw_offset_ += drawcount;
  return true;
}

void MultiDrawManager::ResizeArrays() {
  switch (result_.draw_function) {
    case DrawFunction::DrawArraysInstanced:
      result_.instance_counts.resize(result_.drawcount);
      FALLTHROUGH;
    case DrawFunction::DrawArrays:
      result_.firsts.resize(result_.drawcount);
      result_.counts.resize(result_.drawcount);
      break;
    case DrawFunction::DrawElementsInstanced:
      result_.instance_counts.resize(result_.drawcount);
      FALLTHROUGH;
    case DrawFunction::DrawElements:
      result_.counts.resize(result_.drawcount);
      switch (index_type_) {
        case IndexStorageType::Offset:
          result_.offsets.resize(result_.drawcount);
          break;
        case IndexStorageType::Pointer:
          result_.indices.resize(result_.drawcount);
          break;
      }
      break;
    default:
      NOTREACHED();
  }
}

bool MultiDrawManager::EnsureDrawArraysFunction(DrawFunction draw_function,
                                                GLenum mode) {
  bool first_call = result_.draw_function == DrawFunction::None;
  bool enums_match = result_.mode == mode;
  if (first_call) {
    result_.draw_function = draw_function;
    result_.mode = mode;
    ResizeArrays();
  }
  return first_call || enums_match;
}

bool MultiDrawManager::EnsureDrawElementsFunction(DrawFunction draw_function,
                                                  GLenum mode,
                                                  GLenum type) {
  bool first_call = result_.draw_function == DrawFunction::None;
  bool enums_match = result_.mode == mode && result_.type == type;
  if (first_call) {
    result_.draw_function = draw_function;
    result_.mode = mode;
    result_.type = type;
    ResizeArrays();
  }
  return first_call || enums_match;
}

}  // namespace gles2
}  // namespace gpu
