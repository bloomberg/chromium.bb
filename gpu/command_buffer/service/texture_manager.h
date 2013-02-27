// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_MANAGER_H_

#include <list>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/gpu_export.h"
#include "ui/gl/async_pixel_transfer_delegate.h"
#include "ui/gl/gl_image.h"

namespace gpu {
namespace gles2 {

class GLES2Decoder;
class Display;
class FeatureInfo;
class TextureDefinition;
class TextureManager;

// Info about Textures currently in the system.
class GPU_EXPORT Texture : public base::RefCounted<Texture> {
 public:
  Texture(TextureManager* manager, GLuint service_id);

  GLenum min_filter() const {
    return min_filter_;
  }

  GLenum mag_filter() const {
    return mag_filter_;
  }

  GLenum wrap_s() const {
    return wrap_s_;
  }

  GLenum wrap_t() const {
    return wrap_t_;
  }

  GLenum usage() const {
    return usage_;
  }

  GLenum pool() const {
    return pool_;
  }

  int num_uncleared_mips() const {
    return num_uncleared_mips_;
  }

  uint32 estimated_size() const {
    return estimated_size_;
  }

  bool CanRenderTo() const {
    return !stream_texture_ && target_ != GL_TEXTURE_EXTERNAL_OES;
  }

  // The service side OpenGL id of the texture.
  GLuint service_id() const {
    return service_id_;
  }

  void SetServiceId(GLuint service_id) {
    service_id_ = service_id;
  }

  // Returns the target this texure was first bound to or 0 if it has not
  // been bound. Once a texture is bound to a specific target it can never be
  // bound to a different target.
  GLenum target() const {
    return target_;
  }

  // In GLES2 "texture complete" means it has all required mips for filtering
  // down to a 1x1 pixel texture, they are in the correct order, they are all
  // the same format.
  bool texture_complete() const {
    return texture_complete_;
  }

  // In GLES2 "cube complete" means all 6 faces level 0 are defined, all the
  // same format, all the same dimensions and all width = height.
  bool cube_complete() const {
    return cube_complete_;
  }

  // Whether or not this texture is a non-power-of-two texture.
  bool npot() const {
    return npot_;
  }

  bool SafeToRenderFrom() const {
    return cleared_;
  }

  // Get the width and height for a particular level. Returns false if level
  // does not exist.
  bool GetLevelSize(
      GLint target, GLint level, GLsizei* width, GLsizei* height) const;

  // Get the type of a level. Returns false if level does not exist.
  bool GetLevelType(
      GLint target, GLint level, GLenum* type, GLenum* internal_format) const;

  // Get the image bound to a particular level. Returns NULL if level
  // does not exist.
  gfx::GLImage* GetLevelImage(GLint target, GLint level) const;

  bool IsDeleted() const {
    return deleted_;
  }

  // Returns true of the given dimensions are inside the dimensions of the
  // level and if the format and type match the level.
  bool ValidForTexture(
      GLint target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLsizei width,
      GLsizei height,
      GLenum format,
      GLenum type) const;

  bool IsValid() const {
    return target() && !IsDeleted();
  }

  void SetNotOwned() {
    owned_ = false;
  }

  bool IsAttachedToFramebuffer() const {
    return framebuffer_attachment_count_ != 0;
  }

  void AttachToFramebuffer() {
    ++framebuffer_attachment_count_;
  }

  void DetachFromFramebuffer() {
    DCHECK_GT(framebuffer_attachment_count_, 0);
    --framebuffer_attachment_count_;
  }

  void SetStreamTexture(bool stream_texture) {
    stream_texture_ = stream_texture;
  }

  int IsStreamTexture() {
    return stream_texture_;
  }

  gfx::AsyncPixelTransferState* GetAsyncTransferState() const {
    return async_transfer_state_.get();
  }
  void SetAsyncTransferState(scoped_ptr<gfx::AsyncPixelTransferState> state) {
    async_transfer_state_ = state.Pass();
  }
  bool AsyncTransferIsInProgress() {
    return async_transfer_state_ &&
        async_transfer_state_->TransferIsInProgress();
  }

  void SetImmutable(bool immutable) {
    immutable_ = immutable;
  }

  bool IsImmutable() {
    return immutable_;
  }

  // Whether a particular level/face is cleared.
  bool IsLevelCleared(GLenum target, GLint level);

  // Whether the texture has been defined
  bool IsDefined() {
    return estimated_size() > 0;
  }

 private:
  friend class TextureManager;
  friend class base::RefCounted<Texture>;

  ~Texture();

  struct LevelInfo {
    LevelInfo();
    LevelInfo(const LevelInfo& rhs);
    ~LevelInfo();

    bool cleared;
    GLenum target;
    GLint level;
    GLenum internal_format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum format;
    GLenum type;
    scoped_refptr<gfx::GLImage> image;
    uint32 estimated_size;
  };

  // Set the info for a particular level.
  void SetLevelInfo(
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
      bool cleared);

  // Marks a particular level as cleared or uncleared.
  void SetLevelCleared(GLenum target, GLint level, bool cleared);

  // Updates the cleared flag for this texture by inspecting all the mips.
  void UpdateCleared();

  // Clears any renderable uncleared levels.
  // Returns false if a GL error was generated.
  bool ClearRenderableLevels(GLES2Decoder* decoder);

  // Clears the level.
  // Returns false if a GL error was generated.
  bool ClearLevel(GLES2Decoder* decoder, GLenum target, GLint level);

  // Sets a texture parameter.
  // TODO(gman): Expand to SetParameteri,f,iv,fv
  // Returns GL_NO_ERROR on success. Otherwise the error to generate.
  GLenum SetParameter(
      const FeatureInfo* feature_info, GLenum pname, GLint param);

  // Makes each of the mip levels as though they were generated.
  bool MarkMipmapsGenerated(const FeatureInfo* feature_info);

  void MarkAsDeleted() {
    deleted_ = true;
  }

  bool NeedsMips() const {
    return min_filter_ != GL_NEAREST && min_filter_ != GL_LINEAR;
  }

  // True if this texture meets all the GLES2 criteria for rendering.
  // See section 3.8.2 of the GLES2 spec.
  bool CanRender(const FeatureInfo* feature_info) const;

  // Returns true if mipmaps can be generated by GL.
  bool CanGenerateMipmaps(const FeatureInfo* feature_info) const;

  // Sets the Texture's target
  // Parameters:
  //   target: GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP or
  //           GL_TEXTURE_EXTERNAL_OES or GL_TEXTURE_RECTANGLE_ARB
  //   max_levels: The maximum levels this type of target can have.
  void SetTarget(GLenum target, GLint max_levels);

  // Update info about this texture.
  void Update(const FeatureInfo* feature_info);

  // Set the image for a particular level.
  void SetLevelImage(
      const FeatureInfo* feature_info,
      GLenum target,
      GLint level,
      gfx::GLImage* image);

  // Appends a signature for the given level.
  void AddToSignature(
      const FeatureInfo* feature_info,
      GLenum target, GLint level, std::string* signature) const;

  // Info about each face and level of texture.
  std::vector<std::vector<LevelInfo> > level_infos_;

  // The texture manager that manages this Texture.
  TextureManager* manager_;

  // The id of the texure
  GLuint service_id_;

  // Whether this texture has been deleted.
  bool deleted_;

  // Whether all renderable mips of this texture have been cleared.
  bool cleared_;

  int num_uncleared_mips_;

  // The target. 0 if unset, otherwise GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
  GLenum target_;

  // Texture parameters.
  GLenum min_filter_;
  GLenum mag_filter_;
  GLenum wrap_s_;
  GLenum wrap_t_;
  GLenum usage_;
  GLenum pool_;

  // The maximum level that has been set.
  GLint max_level_set_;

  // Whether or not this texture is "texture complete"
  bool texture_complete_;

  // Whether or not this texture is "cube complete"
  bool cube_complete_;

  // Whether or not this texture is non-power-of-two
  bool npot_;

  // Whether this texture has ever been bound.
  bool has_been_bound_;

  // The number of framebuffers this texture is attached to.
  int framebuffer_attachment_count_;

  // Whether the associated context group owns this texture and should delete
  // it.
  bool owned_;

  // Whether this is a special streaming texture.
  bool stream_texture_;

  // State to facilitate async transfers on this texture.
  scoped_ptr<gfx::AsyncPixelTransferState> async_transfer_state_;

  // Whether the texture is immutable and no further changes to the format
  // or dimensions of the texture object can be made.
  bool immutable_;

  // Size in bytes this texture is assumed to take in memory.
  uint32 estimated_size_;

  DISALLOW_COPY_AND_ASSIGN(Texture);
};

// This class keeps track of the textures and their sizes so we can do NPOT and
// texture complete checking.
//
// NOTE: To support shared resources an instance of this class will need to be
// shared by multiple GLES2Decoders.
class GPU_EXPORT TextureManager {
 public:
  enum DefaultAndBlackTextures {
    kTexture2D,
    kCubeMap,
    kExternalOES,
    kRectangleARB,
    kNumDefaultTextures
  };

  TextureManager(MemoryTracker* memory_tracker,
                 FeatureInfo* feature_info,
                 GLsizei max_texture_size,
                 GLsizei max_cube_map_texture_size);
  ~TextureManager();

  // Init the texture manager.
  bool Initialize();

  // Must call before destruction.
  void Destroy(bool have_context);

  // Returns the maximum number of levels.
  GLint MaxLevelsForTarget(GLenum target) const {
    switch (target) {
      case GL_TEXTURE_2D:
        return  max_levels_;
      case GL_TEXTURE_EXTERNAL_OES:
        return 1;
      default:
        return max_cube_map_levels_;
    }
  }

  // Returns the maximum size.
  GLsizei MaxSizeForTarget(GLenum target) const {
    switch (target) {
      case GL_TEXTURE_2D:
      case GL_TEXTURE_EXTERNAL_OES:
        return max_texture_size_;
      default:
        return max_cube_map_texture_size_;
    }
  }

  // Returns the maxium number of levels a texture of the given size can have.
  static GLsizei ComputeMipMapCount(
    GLsizei width, GLsizei height, GLsizei depth);

  // Checks if a dimensions are valid for a given target.
  bool ValidForTarget(
      GLenum target, GLint level,
      GLsizei width, GLsizei height, GLsizei depth);

  // True if this texture meets all the GLES2 criteria for rendering.
  // See section 3.8.2 of the GLES2 spec.
  bool CanRender(const Texture* texture) const {
    return texture->CanRender(feature_info_);
  }

  // Returns true if mipmaps can be generated by GL.
  bool CanGenerateMipmaps(const Texture* texture) const {
    return texture->CanGenerateMipmaps(feature_info_);
  }

  // Sets the Texture's target
  // Parameters:
  //   target: GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP
  //   max_levels: The maximum levels this type of target can have.
  void SetInfoTarget(
      Texture* info,
      GLenum target);

  // Set the info for a particular level in a TexureInfo.
  void SetLevelInfo(
      Texture* info,
      GLenum target,
      GLint level,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type,
      bool cleared);

  // Save the texture definition and leave it undefined.
  TextureDefinition* Save(Texture* info);

  // Redefine all the levels from the texture definition.
  bool Restore(Texture* info,
               TextureDefinition* definition);

  // Sets a mip as cleared.
  void SetLevelCleared(Texture* info, GLenum target,
                       GLint level, bool cleared);

  // Sets a texture parameter of a Texture
  // Returns GL_NO_ERROR on success. Otherwise the error to generate.
  // TODO(gman): Expand to SetParameteri,f,iv,fv
  GLenum SetParameter(
      Texture* info, GLenum pname, GLint param);

  // Makes each of the mip levels as though they were generated.
  // Returns false if that's not allowed for the given texture.
  bool MarkMipmapsGenerated(Texture* info);

  // Clears any uncleared renderable levels.
  bool ClearRenderableLevels(GLES2Decoder* decoder, Texture* info);

  // Clear a specific level.
  bool ClearTextureLevel(
      GLES2Decoder* decoder,Texture* info, GLenum target, GLint level);

  // Creates a new texture info.
  Texture* CreateTexture(GLuint client_id, GLuint service_id);

  // Gets the texture info for the given texture.
  Texture* GetTexture(GLuint client_id) const;

  // Removes a texture info.
  void RemoveTexture(GLuint client_id);

  // Gets a client id for a given service id.
  bool GetClientId(GLuint service_id, GLuint* client_id) const;

  Texture* GetDefaultTextureInfo(GLenum target) {
    switch (target) {
      case GL_TEXTURE_2D:
        return default_textures_[kTexture2D];
      case GL_TEXTURE_CUBE_MAP:
        return default_textures_[kCubeMap];
      case GL_TEXTURE_EXTERNAL_OES:
        return default_textures_[kExternalOES];
      case GL_TEXTURE_RECTANGLE_ARB:
        return default_textures_[kRectangleARB];
      default:
        NOTREACHED();
        return NULL;
    }
  }

  bool HaveUnrenderableTextures() const {
    return num_unrenderable_textures_ > 0;
  }

  bool HaveUnsafeTextures() const {
    return num_unsafe_textures_ > 0;
  }

  bool HaveUnclearedMips() const {
    return num_uncleared_mips_ > 0;
  }

  GLuint black_texture_id(GLenum target) const {
    switch (target) {
      case GL_SAMPLER_2D:
        return black_texture_ids_[kTexture2D];
      case GL_SAMPLER_CUBE:
        return black_texture_ids_[kCubeMap];
      case GL_SAMPLER_EXTERNAL_OES:
        return black_texture_ids_[kExternalOES];
      case GL_SAMPLER_2D_RECT_ARB:
        return black_texture_ids_[kRectangleARB];
      default:
        NOTREACHED();
        return 0;
    }
  }

  size_t mem_represented() const {
    return
        memory_tracker_managed_->GetMemRepresented() +
        memory_tracker_unmanaged_->GetMemRepresented();
  }

  void SetLevelImage(
      Texture* info,
      GLenum target,
      GLint level,
      gfx::GLImage* image);

  void AddToSignature(
      Texture* info,
      GLenum target,
      GLint level,
      std::string* signature) const;

  // Transfers added will get their Texture updated at the same time
  // the async transfer is bound to the real texture.
  void AddPendingAsyncPixelTransfer(
      base::WeakPtr<gfx::AsyncPixelTransferState> state, Texture* info);
  void BindFinishedAsyncPixelTransfers(bool* texture_dirty,
                                       bool* framebuffer_dirty);

 private:
  friend class Texture;

  // Helper for Initialize().
  scoped_refptr<Texture> CreateDefaultAndBlackTextures(
      GLenum target,
      GLuint* black_texture);

  void StartTracking(Texture* info);
  void StopTracking(Texture* info);

  MemoryTypeTracker* GetMemTracker(GLenum texture_pool);
  scoped_ptr<MemoryTypeTracker> memory_tracker_managed_;
  scoped_ptr<MemoryTypeTracker> memory_tracker_unmanaged_;

  scoped_refptr<FeatureInfo> feature_info_;

  // Info for each texture in the system.
  typedef base::hash_map<GLuint, scoped_refptr<Texture> > TextureInfoMap;
  TextureInfoMap texture_infos_;

  GLsizei max_texture_size_;
  GLsizei max_cube_map_texture_size_;
  GLint max_levels_;
  GLint max_cube_map_levels_;

  int num_unrenderable_textures_;
  int num_unsafe_textures_;
  int num_uncleared_mips_;

  // Counts the number of Texture allocated with 'this' as its manager.
  // Allows to check no Texture will outlive this.
  unsigned int texture_info_count_;

  bool have_context_;

  // Black (0,0,0,1) textures for when non-renderable textures are used.
  // NOTE: There is no corresponding Texture for these textures.
  // TextureInfos are only for textures the client side can access.
  GLuint black_texture_ids_[kNumDefaultTextures];

  // The default textures for each target (texture name = 0)
  scoped_refptr<Texture> default_textures_[kNumDefaultTextures];

  // Async texture allocations which haven't been bound to their textures
  // yet. This facilitates updating the Texture at the same time the
  // real texture data is bound.
  typedef std::pair<base::WeakPtr<gfx::AsyncPixelTransferState>,
                    Texture*> PendingAsyncTransfer;
  typedef std::list<PendingAsyncTransfer> PendingAsyncTransferList;
  PendingAsyncTransferList pending_async_transfers_;

  DISALLOW_COPY_AND_ASSIGN(TextureManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_MANAGER_H_
