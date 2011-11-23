// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_manager.h"
#include "base/bits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {
namespace gles2 {

static GLsizei ComputeMipMapCount(
    GLsizei width, GLsizei height, GLsizei depth) {
  return 1 + base::bits::Log2Floor(std::max(std::max(width, height), depth));
}

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
}

void TextureManager::Destroy(bool have_context) {
  while (!texture_infos_.empty()) {
    if (have_context) {
      TextureInfo* info = texture_infos_.begin()->second;
      if (!info->IsDeleted() && info->owned_) {
        GLuint service_id = info->service_id();
        glDeleteTextures(1, &service_id);
        info->MarkAsDeleted();
      }
    }
    texture_infos_.erase(texture_infos_.begin());
  }
  if (have_context) {
    GLuint ids[] = {
      black_2d_texture_id_,
      black_cube_texture_id_,
      default_texture_2d_->service_id(),
      default_texture_cube_map_->service_id(),
    };
    glDeleteTextures(arraysize(ids), ids);
  }
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
  const TextureInfo::LevelInfo& first = level_infos_[0][0];
  // TODO(gman): Check internal_format, format and type.
  for (size_t ii = 0; ii < level_infos_.size(); ++ii) {
    const LevelInfo& info = level_infos_[ii][0];
    if ((info.target == 0) ||
        (info.width != first.width) ||
        (info.height != first.height) ||
        (info.depth != 1) ||
        (info.format != first.format) ||
        (info.internal_format != first.internal_format) ||
        (info.type != first.type)) {
        return false;
    }
  }
  return true;
}

void TextureManager::TextureInfo::SetLevelCleared(GLenum target, GLint level) {
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
  }
  info.cleared = true;
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
    GLint face,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type) const {
  size_t face_index = GLTargetToFaceIndex(face);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(face)][level];
    GLint right;
    GLint top;
    return SafeAdd(xoffset, width, &right) &&
           SafeAdd(yoffset, height, &top) &&
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
    GLint face, GLint level, GLsizei* width, GLsizei* height) const {
  DCHECK(width);
  DCHECK(height);
  size_t face_index = GLTargetToFaceIndex(face);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(face)][level];
    if (info.target != 0) {
      *width = info.width;
      *height = info.height;
      return true;
    }
  }
  return false;
}

bool TextureManager::TextureInfo::GetLevelType(
    GLint face, GLint level, GLenum* type, GLenum* internal_format) const {
  DCHECK(type);
  DCHECK(internal_format);
  size_t face_index = GLTargetToFaceIndex(face);
  if (level >= 0 && face_index < level_infos_.size() &&
      static_cast<size_t>(level) < level_infos_[face_index].size()) {
    const LevelInfo& info = level_infos_[GLTargetToFaceIndex(face)][level];
    if (info.target != 0) {
      *type = info.type;
      *internal_format = info.internal_format;
      return true;
    }
  }
  return false;
}

bool TextureManager::TextureInfo::SetParameter(
    const FeatureInfo* feature_info, GLenum pname, GLint param) {
  DCHECK(feature_info);

  if (target_ == GL_TEXTURE_EXTERNAL_OES ||
      target_ == GL_TEXTURE_RECTANGLE_ARB) {
    if (pname == GL_TEXTURE_MIN_FILTER &&
        (param != GL_NEAREST && param != GL_LINEAR))
      return false;
    if ((pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T) &&
        param != GL_CLAMP_TO_EDGE)
      return false;
  }

  switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
      if (!feature_info->validators()->texture_min_filter_mode.IsValid(param)) {
        return false;
      }
      min_filter_ = param;
      break;
    case GL_TEXTURE_MAG_FILTER:
      if (!feature_info->validators()->texture_mag_filter_mode.IsValid(param)) {
        return false;
      }
      mag_filter_ = param;
      break;
    case GL_TEXTURE_WRAP_S:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return false;
      }
      wrap_s_ = param;
      break;
    case GL_TEXTURE_WRAP_T:
      if (!feature_info->validators()->texture_wrap_mode.IsValid(param)) {
        return false;
      }
      wrap_t_ = param;
      break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      // Nothing to do for this case at the moment.
      break;
    default:
      NOTREACHED();
      return false;
  }
  Update(feature_info);
  UpdateCleared();
  return true;
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
      info.width, info.height);
  if (!info.cleared) {
    ++num_uncleared_mips_;
  }
  return info.cleared;
}

TextureManager::TextureManager(
    GLint max_texture_size,
    GLint max_cube_map_texture_size)
    : max_texture_size_(max_texture_size),
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
      black_2d_texture_id_(0),
      black_cube_texture_id_(0),
      black_oes_external_texture_id_(0),
      black_arb_texture_rectangle_id_(0) {
}

bool TextureManager::Initialize(const FeatureInfo* feature_info) {
  // TODO(gman): The default textures have to be real textures, not the 0
  // texture because we simulate non shared resources on top of shared
  // resources and all contexts that share resource share the same default
  // texture.

  default_texture_2d_ = CreateDefaultAndBlackTextures(
      feature_info, GL_TEXTURE_2D, &black_2d_texture_id_);
  default_texture_cube_map_ = CreateDefaultAndBlackTextures(
      feature_info, GL_TEXTURE_CUBE_MAP, &black_cube_texture_id_);

  if (feature_info->feature_flags().oes_egl_image_external) {
    default_texture_external_oes_ = CreateDefaultAndBlackTextures(
        feature_info, GL_TEXTURE_EXTERNAL_OES,
        &black_oes_external_texture_id_);
  }

  if (feature_info->feature_flags().arb_texture_rectangle) {
    default_texture_rectangle_arb_ = CreateDefaultAndBlackTextures(
        feature_info, GL_TEXTURE_RECTANGLE_ARB,
        &black_arb_texture_rectangle_id_);
  }

  return true;
}

TextureManager::TextureInfo::Ref TextureManager::CreateDefaultAndBlackTextures(
    const FeatureInfo* feature_info,
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
  TextureInfo::Ref default_texture = TextureInfo::Ref(new TextureInfo(ids[1]));
  SetInfoTarget(feature_info, default_texture, target);
  FeatureInfo temp_feature_info;
  if (needs_faces) {
    for (int ii = 0; ii < GLES2Util::kNumFaces; ++ii) {
      SetLevelInfo(
          &temp_feature_info, default_texture,
          GLES2Util::IndexToGLFaceTarget(ii),
          0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
  } else {
    // TODO(kbr): previous code called SetLevelInfo directly on the
    // TextureInfo object for the GL_TEXTURE_EXTERNAL_OES case.
    // Unclear whether this was deliberate.
    if (needs_initialization) {
      SetLevelInfo(&temp_feature_info, default_texture,
                   GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, true);
    } else {
      default_texture->SetLevelInfo(
          &temp_feature_info, GL_TEXTURE_EXTERNAL_OES, 0,
          GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, true);
    }
  }

  *black_texture = ids[0];
  return default_texture;
}

bool TextureManager::ValidForTarget(
    const FeatureInfo* feature_info,
    GLenum target, GLint level,
    GLsizei width, GLsizei height, GLsizei depth) {
  GLsizei max_size = MaxSizeForTarget(target);
  return level >= 0 &&
         width >= 0 &&
         height >= 0 &&
         depth >= 0 &&
         level < MaxLevelsForTarget(target) &&
         width <= max_size &&
         height <= max_size &&
         depth <= max_size &&
         (level == 0 || feature_info->feature_flags().npot_ok ||
          (!GLES2Util::IsNPOT(width) &&
           !GLES2Util::IsNPOT(height) &&
           !GLES2Util::IsNPOT(depth))) &&
         (target != GL_TEXTURE_CUBE_MAP || (width == height && depth == 1)) &&
         (target != GL_TEXTURE_2D || (depth == 1));
}

void TextureManager::SetInfoTarget(
    const FeatureInfo* feature_info,
    TextureManager::TextureInfo* info, GLenum target) {
  DCHECK(info);
  if (!info->CanRender(feature_info)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  info->SetTarget(target, MaxLevelsForTarget(target));
  if (!info->CanRender(feature_info)) {
    ++num_unrenderable_textures_;
  }
}

void TextureManager::SetLevelCleared(
    TextureManager::TextureInfo* info, GLenum target, GLint level) {
  DCHECK(info);
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  info->SetLevelCleared(target, level);
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
    const FeatureInfo* feature_info,
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
  if (!info->CanRender(feature_info)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  info->SetLevelInfo(
      feature_info, target, level, internal_format, width, height, depth,
      border, format, type, cleared);
  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->CanRender(feature_info)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
}

bool TextureManager::SetParameter(
    const FeatureInfo* feature_info,
    TextureManager::TextureInfo* info, GLenum pname, GLint param) {
  DCHECK(feature_info);
  DCHECK(info);
  if (!info->CanRender(feature_info)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  bool result = info->SetParameter(feature_info, pname, param);
  if (!info->CanRender(feature_info)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  return result;
}

bool TextureManager::MarkMipmapsGenerated(
    const FeatureInfo* feature_info,
    TextureManager::TextureInfo* info) {
  DCHECK(info);
  if (!info->CanRender(feature_info)) {
    DCHECK_NE(0, num_unrenderable_textures_);
    --num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    DCHECK_NE(0, num_unsafe_textures_);
    --num_unsafe_textures_;
  }
  num_uncleared_mips_ -= info->num_uncleared_mips();
  DCHECK_GE(num_uncleared_mips_, 0);
  bool result = info->MarkMipmapsGenerated(feature_info);
  num_uncleared_mips_ += info->num_uncleared_mips();
  if (!info->CanRender(feature_info)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  return result;
}

TextureManager::TextureInfo* TextureManager::CreateTextureInfo(
    const FeatureInfo* feature_info,
    GLuint client_id, GLuint service_id) {
  TextureInfo::Ref info(new TextureInfo(service_id));
  std::pair<TextureInfoMap::iterator, bool> result =
      texture_infos_.insert(std::make_pair(client_id, info));
  DCHECK(result.second);
  if (!info->CanRender(feature_info)) {
    ++num_unrenderable_textures_;
  }
  if (!info->SafeToRenderFrom()) {
    ++num_unsafe_textures_;
  }
  num_uncleared_mips_ += info->num_uncleared_mips();
  return info.get();
}

TextureManager::TextureInfo* TextureManager::GetTextureInfo(
    GLuint client_id) {
  TextureInfoMap::iterator it = texture_infos_.find(client_id);
  return it != texture_infos_.end() ? it->second : NULL;
}

void TextureManager::RemoveTextureInfo(
    const FeatureInfo* feature_info, GLuint client_id) {
  TextureInfoMap::iterator it = texture_infos_.find(client_id);
  if (it != texture_infos_.end()) {
    TextureInfo* info = it->second;
    if (!info->CanRender(feature_info)) {
      DCHECK_NE(0, num_unrenderable_textures_);
      --num_unrenderable_textures_;
    }
    if (!info->SafeToRenderFrom()) {
      DCHECK_NE(0, num_unsafe_textures_);
      --num_unsafe_textures_;
    }
    num_uncleared_mips_ -= info->num_uncleared_mips();
    DCHECK_GE(num_uncleared_mips_, 0);
    info->MarkAsDeleted();
    texture_infos_.erase(it);
  }
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

}  // namespace gles2
}  // namespace gpu


