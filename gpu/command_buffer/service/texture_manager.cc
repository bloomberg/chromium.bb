// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"
#include "base/bits.h"
#include "base/stringprintf.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/texture_definition.h"

namespace gpu {
namespace gles2 {

static size_t GLTargetToFaceIndex(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
    case GL_TEXTURE_EXTERNAL_OES:
    case GL_TEXTURE_RECTANGLE_ARB:
      return 0;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      return 0;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      return 1;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      return 2;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      return 3;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      return 4;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
      return 5;
    default:
      NOTREACHED();
      return 0;
  }
}

static size_t FaceIndexToGLTarget(size_t index) {
  switch (index) {
    case 0:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    case 1:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
    case 2:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
    case 3:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
    case 4:
      return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
    case 5:
      return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    default:
      NOTREACHED();
      return 0;
  }
}

TextureManager::~TextureManager() {
  DCHECK(texture_infos_.empty());

  // If this triggers, that means something is keeping a reference to
  // a TextureInfo belonging to this.
  CHECK_EQ(texture_info_count_, 0u);

  DCHECK_EQ(0, num_unrenderable_textures_);
  DCHECK_EQ(0, num_unsafe_textures_);
  DCHECK_EQ(0, num_uncleared_mips_);
}

void TextureManager::Destroy(bool have_context) {
  have_context_ = have_context;
  texture_infos_.clear();
  for (int ii = 0; ii < kNumDefaultTextures; ++ii) {
    default_textures_[ii] = NULL;
  }

  if (have_context) {
    glDeleteTextures(arraysize(black_texture_ids_), black_texture_ids_);
  }

  DCHECK_EQ(0u, memory_tracker_managed_->GetMemRepresented());
  DCHECK_EQ(0u, memory_tracker_unmanaged_->GetMemRepresented());
}

TextureManager::TextureInfo::TextureInfo(TextureManager* manager,
                                         GLuint service_id)
    : manager_(manager),
      service_id_(service_id),
      deleted_(false),
      cleared_(true),
      num_uncleared_mips_(0),
      target_(0),
      min_filter_(GL_NEAREST_MIPMAP_LINEAR),
      mag_filter_(GL_LINEAR),
      wrap_s_(GL_REPEAT),
      wrap_t_(GL_REPEAT),
      usage_(GL_NONE),
      pool_(GL_TEXTURE_POOL_UNMANAGED_CHROMIUM),
      max_level_set_(-1),
      texture_complete_(false),
      cube_complete_(false),
      npot_(false),
      has_been_bound_(false),
      framebuffer_attachment_count_(0),
      owned_(true),
      stream_texture_(false),
      immutable_(false),
      estimated_size_(0) {
  if (manager_) {
    manager_->StartTracking(this);
  }
}

TextureManager::TextureInfo::~TextureInfo() {
  if (manager_) {
    if (owned_ && manager_->have_context_) {
      GLuint id = service_id();
      glDeleteTextures(1, &id);
    }
    MarkAsDeleted();
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

TextureManager::TextureInfo::LevelInfo::LevelInfo()
    : cleared(true),
      target(0),
      level(-1),
      internal_format(0),
      width(0),
      height(0),
      depth(0),
      border(0),
      format(0),
      type(0),
      estimated_size(0) {
}

TextureManager::TextureInfo::LevelInfo::LevelInfo(const LevelInfo& rhs)
    : cleared(rhs.cleared),
      target(rhs.target),
      level(rhs.level),
      internal_format(rhs.internal_format),
      width(rhs.width),
      height(rhs.height),
      depth(rhs.depth),
      border(rhs.border),
      format(rhs.format),
      type(rhs.type),
      image(rhs.image),
      estimated_size(rhs.estimated_size) {
}

TextureManager::TextureInfo::LevelInfo::~LevelInfo() {
}

bool TextureManager::TextureInfo::CanRender(
    const FeatureInfo* feature_info) const {
  if (target_ == 0) {
    return false;
  }
  bool needs_mips = NeedsMips();
  if ((npot() && !feature_info->feature_flags().npot_ok) ||
      (target_ == GL_TEXTURE_RECTANGLE_ARB)) {
    return !needs_mips &&
           wrap_s_ == GL_CLAMP_TO_EDGE &&
           wrap_t_ == GL_CLAMP_TO_EDGE;
  }
  if (needs_mips) {
    if (target_ == GL_TEXTURE_2D) {
      return texture_complete();
    } else {
      return texture_complete() && cube_complete();
    }
  } else {
    return true;
  }
}

void TextureManager::TextureInfo::AddToSignature(
    const FeatureInfo* feature_info,
    GLenum target,
    GLint level,
    std::string* signature) const {
  DCHECK(feature_info);
  DCHECK(signature);
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(GLTargetToFaceIndex(target)),
            level_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            level_infos_[GLTargetToFaceIndex(target)].size());
  const TextureInfo::LevelInfo& info =
      level_infos_[GLTargetToFaceIndex(target)][level];
  *signature += base::StringPrintf(
      "|Texture|target=%04x|level=%d|internal_format=%04x"
      "|width=%d|height=%d|depth=%d|border=%d|format=%04x|type=%04x"
      "|image=%d|canrender=%d|canrenderto=%d|npot_=%d"
      "|min_filter=%04x|mag_filter=%04x|wrap_s=%04x|wrap_t=%04x"
      "|usage=%04x",
      target, level, info.internal_format,
      info.width, info.height, info.depth, info.border,
      info.format, info.type, info.image.get() != NULL,
      CanRender(feature_info), CanRenderTo(), npot_,
      min_filter_, mag_filter_, wrap_s_, wrap_t_,
      usage_);
}

bool TextureManager::TextureInfo::MarkMipmapsGenerated(
    const FeatureInfo* feature_info) {
  if (!CanGenerateMipmaps(feature_info)) {
    return false;
  }
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const TextureInfo::LevelInfo& info1 = level_infos_[ii][0];
    GLsizei width = info1.width;
    GLsizei height = info1.height;
    GLsizei depth = info1.depth;
    GLenum target = target_ == GL_TEXTURE_2D ? GL_TEXTURE_2D :
                               FaceIndexToGLTarget(ii);
    int num_mips = ComputeMipMapCount(width, height, depth);
    for (int level = 1; level < num_mips; ++level) {
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = std::max(1, depth >> 1);
      SetLevelInfo(feature_info,
                   target,
                   level,
                   info1.internal_format,
                   width,
                   height,
                   depth,
                   info1.border,
                   info1.format,
                   info1.type,
                   true);
    }
  }

  return true;
}

void TextureManager::TextureInfo::SetTarget(GLenum target, GLint max_levels) {
  DCHECK_EQ(0u, target_);  // you can only set this once.
  target_ = target;
  size_t num_faces = (target == GL_TEXTURE_CUBE_MAP) ? 6 : 1;
  level_infos_.resize(num_faces);
  for (size_t ii = 0; ii < num_faces; ++ii) {
    level_infos_[ii].resize(max_levels);
  }

  if (target == GL_TEXTURE_EXTERNAL_OES || target == GL_TEXTURE_RECTANGLE_ARB) {
    min_filter_ = GL_LINEAR;
    wrap_s_ = wrap_t_ = GL_CLAMP_TO_EDGE;
  }
}

bool TextureManager::TextureInfo::CanGenerateMipmaps(
    const FeatureInfo* feature_info) const {
  if ((npot() && !feature_info->feature_flags().npot_ok) ||
      level_infos_.empty() ||
      target_ == GL_TEXTURE_EXTERNAL_OES ||
      target_ == GL_TEXTURE_RECTANGLE_ARB) {
    return false;
  }

  // Can't generate mips for depth or stencil textures.
  const TextureInfo::LevelInfo& first = level_infos_[0][0];
  uint32 channels = GLES2Util::GetChannelsForFormat(first.format);
  if (channels & (GLES2Util::kDepth | GLES2Util::kStencil)) {
    return false;
  }

  // TODO(gman): Check internal_format, format and type.
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const LevelInfo& info = level_infos_[ii][0];
    if ((info.target == 0) ||
        (info.width != first.width) ||
        (info.height != first.height) ||
        (info.depth != 1) ||
        (info.format != first.format) ||
        (info.internal_format != first.internal_format) ||
        (info.type != first.type) ||
        feature_info->validators()->compressed_texture_format.IsValid(
            info.internal_format) ||
        info.image) {
        return false;
    }
  }
  return true;
}

void TextureManager::TextureInfo::SetLevelCleared(GLenum target,
                                                  GLint level,
                                                  bool cleared) {
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(GLTargetToFaceIndex(target)),
            level_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            level_infos_[GLTargetToFaceIndex(target)].size());
  TextureInfo::LevelInfo& info =
      level_infos_[GLTargetToFaceIndex(target)][level];
  if (!info.cleared) {
    DCHECK_NE(0, num_uncleared_mips_);
    --num_uncleared_mips_;
  } else {
    ++num_uncleared_mips_;
  }
  info.cleared = cleared;
  UpdateCleared();
}

void TextureManager::TextureInfo::UpdateCleared() {
  if (level_infos_.empty()) {
    return;
  }

  const TextureInfo::LevelInfo& first_face = level_infos_[0][0];
  int levels_needed = ComputeMipMapCount(
      first_face.width, first_face.height, first_face.depth);
  cleared_ = true;
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    for (GLint jj = 0; jj < levels_needed; ++jj) {
      const TextureInfo::LevelInfo& info = level_infos_[ii][jj];
      if (info.width > 0 && info.height > 0 && info.depth > 0 &&
          !info.cleared) {
        cleared_ = false;
        return;
      }
    }
  }
}

void TextureManager::TextureInfo::SetLevelInfo(
    const FeatureInfo* feature_info,
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    bool cleared) {
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(GLTargetToFaceIndex(target)),
            level_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            level_infos_[GLTargetToFaceIndex(target)].size());
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  DCHECK_GE(depth, 0);
  TextureInfo::LevelInfo& info =
      level_infos_[GLTargetToFaceIndex(target)][level];
  info.target = target;
  info.level = level;
  info.internal_format = internal_format;
  info.width = width;
  info.height = height;
  info.depth = depth;
  info.border = border;
  info.format = format;
  info.type = type;
  info.image = 0;

  estimated_size_ -= info.estimated_size;
  GLES2Util::ComputeImageDataSizes(
      width, height, format, type, 4, &info.estimated_size, NULL, NULL);
  estimated_size_ += info.estimated_size;

  if (!info.cleared) {
    DCHECK_NE(0, num_uncleared_mips_);
    --num_uncleared_mips_;
  }
  info.cleared = cleared;
  if (!info.cleared) {
    ++num_uncleared_mips_;
  }
  max_level_set_ = std::max(max_level_set_, level);
  Update(feature_info);
  UpdateCleared();
}

bool TextureManager::TextureInfo::ValidForTexture(
    GLint target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type) const {
  size_t face_index = GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(target)][level];
    int32 right;
    int32 top;
    return SafeAddInt32(xoffset, width, &right) &&
           SafeAddInt32(yoffset, height, &top) &&
           xoffset >= 0 &&
           yoffset >= 0 &&
           right <= info.width &&
           top <= info.height &&
           format == info.internal_format &&
           type == info.type;
  }
  return false;
}

bool TextureManager::TextureInfo::GetLevelSize(
    GLint target, GLint level, GLsizei* width, GLsizei* height) const {
  DCHECK(width);
  DCHECK(height);
  size_t face_index = GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(target)][level];
    if (info.target != 0) {
      *width = info.width;
      *height = info.height;
      return true;
    }
  }
  return false;
}

bool TextureManager::TextureInfo::GetLevelType(
    GLint target, GLint level, GLenum* type, GLenum* internal_format) const {
  DCHECK(type);
  DCHECK(internal_format);
  size_t face_index = GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(target)][level];
    if (info.target != 0) {
      *type = info.type;
      *internal_format = info.internal_format;
      return true;
    }
  }
  return false;
}

GLenum TextureManager::TextureInfo::SetParameter(
    const FeatureInfo* feature_info, GLenum pname, GLint param) {
  DCHECK(feature_info);

  if (target_ == GL_TEXTURE_EXTERNAL_OES ||
      target_ == GL_TEXTURE_RECTANGLE_ARB) {
    if (pname == GL_TEXTURE_MIN_FILTER &&
        (param != GL_NEAREST && param != GL_LINEAR))
      return GL_INVALID_ENUM;
    if ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T) &&
        param != GL_CLAMP_TO_EDGE)
      return GL_INVALID_ENUM;
  }

  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      if (!feature_info->validators()->texture_min_filter_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      min_filter_ = param;
      break;
    case GL_TEXTURE_MAG_FILTER:
      if (!feature_info->validators()->texture_mag_filter_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      mag_filter_ = param;
      break;
    case GL_TEXTURE_POOL_CHROMIUM:
      if (!feature_info->validators()->texture_pool.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      manager_->GetMemTracker(pool_)->TrackMemFree(estimated_size());
      pool_ = param;
      manager_->GetMemTracker(pool_)->TrackMemAlloc(estimated_size());
      break;
    case GL_TEXTURE_WRAP_S:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      wrap_s_ = param;
      break;
    case GL_TEXTURE_WRAP_T:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      wrap_t_ = param;
      break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      if (param < 1) {
        return GL_INVALID_VALUE;
      }
      break;
    case GL_TEXTURE_USAGE_ANGLE:
      if (!feature_info->validators()->texture_usage.IsValid(param)) {
        return GL_INVALID_ENUM;
      }
      usage_ = param;
      break;
    default:
      NOTREACHED();
      return GL_INVALID_ENUM;
  }
  Update(feature_info);
  UpdateCleared();
  return GL_NO_ERROR;
}

void TextureManager::TextureInfo::Update(const FeatureInfo* feature_info) {
  // Update npot status.
  npot_ = false;

  if (level_infos_.empty()) {
    texture_complete_ = false;
    cube_complete_ = false;
    return;
  }

  // checks that the first mip of any face is npot.
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const TextureInfo::LevelInfo& info = level_infos_[ii][0];
    if (GLES2Util::IsNPOT(info.width) ||
        GLES2Util::IsNPOT(info.height) ||
        GLES2Util::IsNPOT(info.depth)) {
      npot_ = true;
      break;
    }
  }

  // Update texture_complete and cube_complete status.
  const TextureInfo::LevelInfo& first_face = level_infos_[0][0];
  int levels_needed = ComputeMipMapCount(
      first_face.width, first_face.height, first_face.depth);
  texture_complete_ =
      max_level_set_ >= (levels_needed - 1) && max_level_set_ >= 0;
  cube_complete_ = (level_infos_.size() == 6) &&
                   (first_face.width == first_face.height);
  if (first_face.width == 0 || first_face.height == 0) {
    texture_complete_ = false;
  }
  if (first_face.type == GL_FLOAT &&
      !feature_info->feature_flags().enable_texture_float_linear &&
      (min_filter_ != GL_NEAREST_MIPMAP_NEAREST ||
       mag_filter_ != GL_NEAREST)) {
    texture_complete_ = false;
  } else if (first_face.type == GL_HALF_FLOAT_OES &&
             !feature_info->feature_flags().enable_texture_half_float_linear &&
             (min_filter_ != GL_NEAREST_MIPMAP_NEAREST ||
              mag_filter_ != GL_NEAREST)) {
    texture_complete_ = false;
  }
  for (size_t ii = 0;
       ii < level_infos_.size() && (cube_complete_ || texture_complete_);
       ++ii) {
    const TextureInfo::LevelInfo& level0 = level_infos_[ii][0];
    if (level0.target == 0 ||
        level0.width != first_face.width ||
        level0.height != first_face.height ||
        level0.depth != 1 ||
        level0.internal_format != first_face.internal_format ||
        level0.format != first_face.format ||
        level0.type != first_face.type) {
      cube_complete_ = false;
    }
    // Get level0 dimensions
    GLsizei width = level0.width;
    GLsizei height = level0.height;
    GLsizei depth = level0.depth;
    for (GLint jj = 1; jj < levels_needed; ++jj) {
      // compute required size for mip.
      width = std::max(1, width >> 1);
      height = std::max(1, height >> 1);
      depth = std::max(1, depth >> 1);
      const TextureInfo::LevelInfo& info = level_infos_[ii][jj];
      if (info.target == 0 ||
          info.width != width ||
          info.height != height ||
          info.depth != depth ||
          info.internal_format != level0.internal_format ||
          info.format != level0.format ||
          info.type != level0.type) {
        texture_complete_ = false;
        break;
      }
    }
  }
}

bool TextureManager::TextureInfo::ClearRenderableLevels(GLES2Decoder* decoder) {
  DCHECK(decoder);
  if (SafeToRenderFrom()) {
    return true;
  }

  const TextureInfo::LevelInfo& first_face = level_infos_[0][0];
  int levels_needed = ComputeMipMapCount(
      first_face.width, first_face.height, first_face.depth);

  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    for (GLint jj = 0; jj < levels_needed; ++jj) {
      TextureInfo::LevelInfo& info = level_infos_[ii][jj];
      if (info.target != 0) {
        if (!ClearLevel(decoder, info.target, jj)) {
          return false;
        }
      }
    }
  }
  cleared_ = true;
  return true;
}

bool TextureManager::TextureInfo::IsLevelCleared(GLenum target, GLint level) {
  size_t face_index = GLTargetToFaceIndex(target);
  if (face_index >= level_infos_.size() ||
      level >= static_cast<GLint>(level_infos_[face_index].size())) {
    return true;
  }

  TextureInfo::LevelInfo& info = level_infos_[face_index][level];

  return info.cleared;
}

bool TextureManager::TextureInfo::ClearLevel(
    GLES2Decoder* decoder, GLenum target, GLint level) {
  DCHECK(decoder);
  size_t face_index = GLTargetToFaceIndex(target);
  if (face_index >= level_infos_.size() ||
      level >= static_cast<GLint>(level_infos_[face_index].size())) {
    return true;
  }

  TextureInfo::LevelInfo& info = level_infos_[face_index][level];

  DCHECK(target == info.target);

  if (info.target == 0 ||
      info.cleared ||
      info.width == 0 ||
      info.height == 0 ||
      info.depth == 0) {
    return true;
  }

  DCHECK_NE(0, num_uncleared_mips_);
  --num_uncleared_mips_;

  // NOTE: It seems kind of gross to call back into the decoder for this
  // but only the decoder knows all the state (like unpack_alignment_) that's
  // needed to be able to call GL correctly.
  info.cleared = decoder->ClearLevel(
      service_id_, target_, info.target, info.level, info.format, info.type,
      info.width, info.height, immutable_);
  if (!info.cleared) {
    ++num_uncleared_mips_;
  }
  return info.cleared;
}

void TextureManager::TextureInfo::SetLevelImage(
    const FeatureInfo* feature_info,
    GLenum target,
    GLint level,
    gfx::GLImage* image) {
  DCHECK_GE(level, 0);
  DCHECK_LT(static_cast<size_t>(GLTargetToFaceIndex(target)),
            level_infos_.size());
  DCHECK_LT(static_cast<size_t>(level),
            level_infos_[GLTargetToFaceIndex(target)].size());
  TextureInfo::LevelInfo& info =
      level_infos_[GLTargetToFaceIndex(target)][level];
  DCHECK_EQ(info.target, target);
  DCHECK_EQ(info.level, level);
  info.image = image;
}

gfx::GLImage* TextureManager::TextureInfo::GetLevelImage(
  GLint target, GLint level) const {
  size_t face_index = GLTargetToFaceIndex(target);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(target)][level];
    if (info.target != 0) {
      return info.image;
    }
  }
  return 0;
}

TextureManager::TextureManager(
    MemoryTracker* memory_tracker,
    FeatureInfo* feature_info,
    GLint max_texture_size,
    GLint max_cube_map_texture_size)
    : memory_tracker_managed_(
          new MemoryTypeTracker(memory_tracker, MemoryTracker::kManaged)),
      memory_tracker_unmanaged_(
          new MemoryTypeTracker(memory_tracker, MemoryTracker::kUnmanaged)),
      feature_info_(feature_info),
      max_texture_size_(max_texture_size),
      max_cube_map_texture_size_(max_cube_map_texture_size),
      max_levels_(ComputeMipMapCount(max_texture_size,
                                     max_texture_size,
                                     max_texture_size)),
      max_cube_map_levels_(ComputeMipMapCount(max_cube_map_texture_size,
                                              max_cube_map_texture_size,
                                              max_cube_map_texture_size)),
      num_unrenderable_textures_(0),
      num_unsafe_textures_(0),
      num_uncleared_mips_(0),
      texture_info_count_(0),
      have_context_(true) {
  for (int ii = 0; ii < kNumDefaultTextures; ++ii) {
    black_texture_ids_[ii] = 0;
  }
}

bool TextureManager::Initialize() {
  // TODO(gman): The default textures have to be real textures, not the 0
  // texture because we simulate non shared resources on top of shared
  // resources and all contexts that share resource share the same default
  // texture.
  default_textures_[kTexture2D] = CreateDefaultAndBlackTextures(
      GL_TEXTURE_2D, &black_texture_ids_[kTexture2D]);
  default_textures_[kCubeMap] = CreateDefaultAndBlackTextures(
      GL_TEXTURE_CUBE_MAP, &black_texture_ids_[kCubeMap]);

  if (feature_info_->feature_flags().oes_egl_image_external) {
    default_textures_[kExternalOES] = CreateDefaultAndBlackTextures(
        GL_TEXTURE_EXTERNAL_OES, &black_texture_ids_[kExternalOES]);
  }

  if (feature_info_->feature_flags().arb_texture_rectangle) {
    default_textures_[kRectangleARB] = CreateDefaultAndBlackTextures(
        GL_TEXTURE_RECTANGLE_ARB, &black_texture_ids_[kRectangleARB]);
  }

  return true;
}

TextureManager::TextureInfo::Ref TextureManager::CreateDefaultAndBlackTextures(
    GLenum target,
    GLuint* black_texture) {
  static uint8 black[] = {0, 0, 0, 255};

  // Sampling a texture not associated with any EGLImage sibling will return
  // black values according to the spec.
  bool needs_initialization = (target != GL_TEXTURE_EXTERNAL_OES);
  bool needs_faces = (target == GL_TEXTURE_CUBE_MAP);

  // Make default textures and texture for replacing non-renderable textures.
  GLuint ids[2];
  glGenTextures(arraysize(ids), ids);
  for (unsigned long ii = 0; ii < arraysize(ids); ++ii) {
    glBindTexture(target, ids[ii]);
    if (needs_initialization) {
      if (needs_faces) {
        for (int jj = 0; jj < GLES2Util::kNumFaces; ++jj) {
          glTexImage2D(GLES2Util::IndexToGLFaceTarget(jj), 0, GL_RGBA, 1, 1, 0,
                       GL_RGBA, GL_UNSIGNED_BYTE, black);
        }
      } else {
        glTexImage2D(target, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, black);
      }
    }
  }
  glBindTexture(target, 0);

  // Since we are manually setting up these textures
  // we need to manually manipulate some of the their bookkeeping.
  ++num_unrenderable_textures_;
  TextureInfo::Ref default_texture = TextureInfo::Ref(
      new TextureInfo(this, ids[1]));
  SetInfoTarget(default_texture, target);
  if (needs_faces) {
    for (int ii = 0; ii < GLES2Util::kNumFaces; ++ii) {
      SetLevelInfo(
          default_texture, GLES2Util::IndexToGLFaceTarget(ii),
          0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
  } else {
    if (needs_initialization) {
      SetLevelInfo(default_texture,
                   GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, true);
    } else {
      SetLevelInfo(
          default_texture, GL_TEXTURE_EXTERNAL_OES, 0,
          GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
  }

  *black_texture = ids[0];
  return default_texture;
}

bool TextureManager::ValidForTarget(
    GLenum target, GLint level, GLsizei width, GLsizei height, GLsizei depth) {
  GLsizei max_size = MaxSizeForTarget(target) >> level;
  return level >= 0 &&
         width >= 0 &&
         height >= 0 &&
         depth >= 0 &&
         level < MaxLevelsForTarget(target) &&
         width <= max_size &&
         height <= max_size &&
         depth <= max_size &&
         (level == 0 || feature_info_->feature_flags().npot_ok ||
          (!GLES2Util::IsNPOT(width) &&
           !GLES2Util::IsNPOT(height) &&
           !GLES2Util::IsNPOT(depth))) &&
         (target != GL_TEXTURE_CUBE_MAP || (width == height && depth == 1)) &&
         (target != GL_TEXTURE_2D || (depth == 1));
}

void TextureManager::SetInfoTarget(
    TextureManager::TextureInfo* info, GLenum target) {
  DCHECK(info);
  if (!info->CanRender(feature_info_)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  info->SetTarget(target, MaxLevelsForTarget(target));
  if (!info->CanRender(feature_info_)) {
    ++num_unrenderable_textures_;
  }
}

void TextureManager::SetLevelCleared(TextureManager::TextureInfo* info,
                                     GLenum target,
                                     GLint level,
                                     bool cleared) {
  DCHECK(info);
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  info->SetLevelCleared(target, level, cleared);
  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
}

bool TextureManager::ClearRenderableLevels(
    GLES2Decoder* decoder,TextureManager::TextureInfo* info) {
  DCHECK(info);
  if (info->SafeToRenderFrom()) {
    return true;
  }
  DCHECK_NE(0, num_unsafe_textures_);
  --num_unsafe_textures_;
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  bool result = info->ClearRenderableLevels(decoder);
  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  return result;
}

bool TextureManager::ClearTextureLevel(
    GLES2Decoder* decoder,TextureManager::TextureInfo* info,
    GLenum target, GLint level) {
  DCHECK(info);
  if (info->num_uncleared_mips() == 0) {
    return true;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  bool result = info->ClearLevel(decoder, target, level);
  info->UpdateCleared();
  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  return result;
}

void TextureManager::SetLevelInfo(
    TextureManager::TextureInfo* info,
    GLenum target,
    GLint level,
    GLenum internal_format,
    GLsizei width,
    GLsizei height,
    GLsizei depth,
    GLint border,
    GLenum format,
    GLenum type,
    bool cleared) {
  DCHECK(info);
  if (!info->CanRender(feature_info_)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);

  GetMemTracker(info->pool_)->TrackMemFree(info->estimated_size());
  info->SetLevelInfo(
      feature_info_, target, level, internal_format, width, height, depth,
      border, format, type, cleared);
  GetMemTracker(info->pool_)->TrackMemAlloc(info->estimated_size());

  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->CanRender(feature_info_)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
}

TextureDefinition* TextureManager::Save(TextureInfo* info) {
  DCHECK(info->owned_);

  if (info->IsAttachedToFramebuffer())
    return NULL;

  TextureDefinition::LevelInfos level_infos(info->level_infos_.size());
  for (size_t face = 0; face < level_infos.size(); ++face) {
    GLenum target = info->target() == GL_TEXTURE_2D ?
        GL_TEXTURE_2D : FaceIndexToGLTarget(face);
    for (size_t level = 0; level < info->level_infos_[face].size(); ++level) {
      const TextureInfo::LevelInfo& level_info =
          info->level_infos_[face][level];
      level_infos[face].push_back(
          TextureDefinition::LevelInfo(target,
                                       level_info.internal_format,
                                       level_info.width,
                                       level_info.height,
                                       level_info.depth,
                                       level_info.border,
                                       level_info.format,
                                       level_info.type,
                                       level_info.cleared));

      SetLevelInfo(info,
                   target,
                   level,
                   GL_RGBA,
                   0,
                   0,
                   0,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   true);
    }
  }

  GLuint old_service_id = info->service_id();
  bool immutable = info->IsImmutable();

  GLuint new_service_id = 0;
  glGenTextures(1, &new_service_id);
  info->SetServiceId(new_service_id);
  info->SetImmutable(false);

  return new TextureDefinition(info->target(),
                               old_service_id,
                               info->min_filter(),
                               info->mag_filter(),
                               info->wrap_s(),
                               info->wrap_t(),
                               info->usage(),
                               immutable,
                               level_infos);
}

bool TextureManager::Restore(TextureInfo* info,
                             TextureDefinition* definition) {
  DCHECK(info->owned_);

  scoped_ptr<TextureDefinition> scoped_definition(definition);

  if (info->IsAttachedToFramebuffer())
    return false;

  if (info->target() != definition->target())
    return false;

  if (info->level_infos_.size() != definition->level_infos().size())
    return false;

  if (info->level_infos_[0].size() != definition->level_infos()[0].size())
    return false;

  for (size_t face = 0; face < info->level_infos_.size(); ++face) {
    GLenum target = info->target() == GL_TEXTURE_2D ?
        GL_TEXTURE_2D : FaceIndexToGLTarget(face);
    for (size_t level = 0; level < info->level_infos_[face].size(); ++level) {
      const TextureDefinition::LevelInfo& level_info =
          definition->level_infos()[face][level];
      SetLevelInfo(info,
                   target,
                   level,
                   level_info.internal_format,
                   level_info.width,
                   level_info.height,
                   level_info.depth,
                   level_info.border,
                   level_info.format,
                   level_info.type,
                   level_info.cleared);
    }
  }

  GLuint old_service_id = info->service_id();
  glDeleteTextures(1, &old_service_id);
  info->SetServiceId(definition->ReleaseServiceId());
  info->SetImmutable(definition->immutable());
  SetParameter(info, GL_TEXTURE_MIN_FILTER, definition->min_filter());
  SetParameter(info, GL_TEXTURE_MAG_FILTER, definition->mag_filter());
  SetParameter(info, GL_TEXTURE_WRAP_S, definition->wrap_s());
  SetParameter(info, GL_TEXTURE_WRAP_T, definition->wrap_t());
  SetParameter(info, GL_TEXTURE_USAGE_ANGLE, definition->usage());

  return true;
}

GLenum TextureManager::SetParameter(
    TextureManager::TextureInfo* info, GLenum pname, GLint param) {
  DCHECK(info);
  if (!info->CanRender(feature_info_)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  GLenum result = info->SetParameter(feature_info_, pname, param);
  if (!info->CanRender(feature_info_)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  return result;
}

bool TextureManager::MarkMipmapsGenerated(TextureManager::TextureInfo* info) {
  DCHECK(info);
  if (!info->CanRender(feature_info_)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  GetMemTracker(info->pool_)->TrackMemFree(info->estimated_size());
  bool result = info->MarkMipmapsGenerated(feature_info_);
  GetMemTracker(info->pool_)->TrackMemAlloc(info->estimated_size());

  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->CanRender(feature_info_)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  return result;
}

TextureManager::TextureInfo* TextureManager::CreateTextureInfo(
    GLuint client_id, GLuint service_id) {
  DCHECK_NE(0u, service_id);
  TextureInfo::Ref info(new TextureInfo(this, service_id));
  std::pair<TextureInfoMap::iterator, bool> result =
      texture_infos_.insert(std::make_pair(client_id, info));
  DCHECK(result.second);
  if (!info->CanRender(feature_info_)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  num_uncleared_mips_ += info->num_uncleared_mips();
  return info.get();
}

TextureManager::TextureInfo* TextureManager::GetTextureInfo(
    GLuint client_id) const {
  TextureInfoMap::const_iterator it = texture_infos_.find(client_id);
  return it != texture_infos_.end() ? it->second : NULL;
}

void TextureManager::RemoveTextureInfo(GLuint client_id) {
  TextureInfoMap::iterator it = texture_infos_.find(client_id);
  if (it != texture_infos_.end()) {
    TextureInfo* info = it->second;
    info->MarkAsDeleted();
    texture_infos_.erase(it);
  }
}

void TextureManager::StartTracking(TextureManager::TextureInfo* /* texture */) {
  ++texture_info_count_;
}

void TextureManager::StopTracking(TextureManager::TextureInfo* texture) {
  --texture_info_count_;
  if (!texture->CanRender(feature_info_)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!texture->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= texture->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  GetMemTracker(texture->pool_)->TrackMemFree(texture->estimated_size());
}

MemoryTypeTracker* TextureManager::GetMemTracker(GLenum tracking_pool) {
  switch(tracking_pool) {
    case GL_TEXTURE_POOL_MANAGED_CHROMIUM:
      return memory_tracker_managed_.get();
      break;
    case GL_TEXTURE_POOL_UNMANAGED_CHROMIUM:
      return memory_tracker_unmanaged_.get();
      break;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}

bool TextureManager::GetClientId(GLuint service_id, GLuint* client_id) const {
  // This doesn't need to be fast. It's only used during slow queries.
  for (TextureInfoMap::const_iterator it = texture_infos_.begin();
       it != texture_infos_.end(); ++it) {
    if (it->second->service_id() == service_id) {
      *client_id = it->first;
      return true;
    }
  }
  return false;
}

GLsizei TextureManager::ComputeMipMapCount(
    GLsizei width, GLsizei height, GLsizei depth) {
  return 1 + base::bits::Log2Floor(std::max(std::max(width, height), depth));
}

void TextureManager::SetLevelImage(
    TextureManager::TextureInfo* info,
    GLenum target,
    GLint level,
    gfx::GLImage* image) {
  DCHECK(info);
  if (!info->CanRender(feature_info_)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  info->SetLevelImage(feature_info_, target, level, image);
  if (!info->CanRender(feature_info_)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
}

void TextureManager::AddToSignature(
    TextureInfo* info,
    GLenum target,
    GLint level,
    std::string* signature) const {
  info->AddToSignature(feature_info_.get(), target, level, signature);
}

void TextureManager::AddPendingAsyncPixelTransfer(
    base::WeakPtr<gfx::AsyncPixelTransferState> state, TextureInfo* info) {
  pending_async_transfers_.push_back(PendingAsyncTransfer(state,info));
}

void TextureManager::BindFinishedAsyncPixelTransfers(
    bool* texture_dirty, bool* framebuffer_dirty) {
  DCHECK(texture_dirty);
  DCHECK(framebuffer_dirty);
  *texture_dirty = false;
  *framebuffer_dirty = false;

  // Remove finished transfers from the list, while
  // marking whether texture unit 0 or frame_buffer status is dirty.
  while(!pending_async_transfers_.empty()) {
    PendingAsyncTransfer state_info = pending_async_transfers_.front();
    if (!state_info.first.get()) {
      // The AsyncState is owned by the TextureInfo. So if the
      // async state is deleted, so is the TextureInfo.
      pending_async_transfers_.pop_front();
      continue;
    }
    // Terminate early, as all transfers finish in order.
    if (state_info.first->TransferIsInProgress())
      break;
    // If the transfer is finished, bind it to the texture,
    // update the TextureInfo, and remove it from pending list.
    *texture_dirty = true;
    *framebuffer_dirty |= state_info.second->IsAttachedToFramebuffer();
    gfx::AsyncTexImage2DParams tex_define_params;
    state_info.second->
        GetAsyncTransferState()->BindTransfer(&tex_define_params);
    SetLevelInfo(
        state_info.second,
        tex_define_params.target,
        tex_define_params.level,
        tex_define_params.internal_format,
        tex_define_params.width,
        tex_define_params.height,
        1, // depth
        tex_define_params.border,
        tex_define_params.format,
        tex_define_params.type,
        true); // cleared
    pending_async_transfers_.pop_front();
  }
}

}  // namespace gles2
}  // namespace gpu
