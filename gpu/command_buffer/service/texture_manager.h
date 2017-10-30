// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/sampler_manager.h"
#include "gpu/gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_image.h"

namespace gpu {
class ServiceDiscardableManager;

namespace gles2 {
class GLES2Decoder;
class GLStreamTextureImage;
struct ContextState;
struct DecoderFramebufferState;
class ErrorState;
class FeatureInfo;
class FramebufferManager;
class MailboxManager;
class ProgressReporter;
class Texture;
class TextureManager;
class TextureRef;

class GPU_EXPORT TextureBase {
 public:
  explicit TextureBase(GLuint service_id);
  virtual ~TextureBase();

  // The service side OpenGL id of the texture.
  GLuint service_id() const { return service_id_; }

  // Returns the target this texure was first bound to or 0 if it has not
  // been bound. Once a texture is bound to a specific target it can never be
  // bound to a different target.
  GLenum target() const { return target_; }

 protected:
  // The id of the texture.
  GLuint service_id_;

  // The target. 0 if unset, otherwise GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
  //             Or GL_TEXTURE_2D_ARRAY or GL_TEXTURE_3D (for GLES3).
  GLenum target_;

  void SetTarget(GLenum target);

  void DeleteFromMailboxManager();

 private:
  friend class MailboxManagerSync;
  friend class MailboxManagerImpl;

  void SetMailboxManager(MailboxManager* mailbox_manager);

  MailboxManager* mailbox_manager_;
};

// A ref-counted version of the TextureBase class that deletes the texture after
// all references have been released.
class TexturePassthrough final : public TextureBase,
                                 public base::RefCounted<TexturePassthrough> {
 public:
  TexturePassthrough(GLuint service_id, GLenum target);

  // Notify the texture that the context is lost and it shouldn't delete the
  // native GL texture in the destructor
  void MarkContextLost();

  void SetLevelImage(GLenum target, GLint level, gl::GLImage* image);
  gl::GLImage* GetLevelImage(GLenum target, GLint level) const;

 protected:
  ~TexturePassthrough() override;

 private:
  friend class base::RefCounted<TexturePassthrough>;

  bool have_context_;

  // Bound images divided into faces and then levels
  std::vector<std::vector<scoped_refptr<gl::GLImage>>> level_images_;

  DISALLOW_COPY_AND_ASSIGN(TexturePassthrough);
};

// Info about Textures currently in the system.
// This class wraps a real GL texture, keeping track of its meta-data. It is
// jointly owned by possibly multiple TextureRef.
class GPU_EXPORT Texture final : public TextureBase {
 public:
  enum ImageState {
    // If an image is associated with the texture and image state is UNBOUND,
    // then sampling out of the texture or using it as a target for drawing
    // will not read/write from/to the image.
    UNBOUND,
    // If image state is BOUND, then sampling from the texture will return the
    // contents of the image and using it as a target will modify the image.
    BOUND,
    // Image state is set to COPIED if the contents of the image has been
    // copied to the texture. Sampling from the texture will be equivalent
    // to sampling out the image (assuming image has not been changed since
    // it was copied). Using the texture as a target for drawing will only
    // modify the texture and not the image.
    COPIED
  };

  struct CompatibilitySwizzle {
    GLenum format;
    GLenum dest_format;
    GLenum red;
    GLenum green;
    GLenum blue;
    GLenum alpha;
  };

  explicit Texture(GLuint service_id);

  const SamplerState& sampler_state() const {
    return sampler_state_;
  }

  GLenum min_filter() const {
    return sampler_state_.min_filter;
  }

  GLenum mag_filter() const {
    return sampler_state_.mag_filter;
  }

  GLenum wrap_r() const {
    return sampler_state_.wrap_r;
  }

  GLenum wrap_s() const {
    return sampler_state_.wrap_s;
  }

  GLenum wrap_t() const {
    return sampler_state_.wrap_t;
  }

  GLenum usage() const {
    return usage_;
  }

  GLenum compare_func() const {
    return sampler_state_.compare_func;
  }

  GLenum compare_mode() const {
    return sampler_state_.compare_mode;
  }

  GLfloat max_lod() const {
    return sampler_state_.max_lod;
  }

  GLfloat min_lod() const {
    return sampler_state_.min_lod;
  }

  GLint base_level() const {
    return base_level_;
  }

  GLint max_level() const {
    return max_level_;
  }

  GLenum swizzle_r() const { return swizzle_r_; }

  GLenum swizzle_g() const { return swizzle_g_; }

  GLenum swizzle_b() const { return swizzle_b_; }

  GLenum swizzle_a() const { return swizzle_a_; }

  int num_uncleared_mips() const {
    return num_uncleared_mips_;
  }

  uint32_t estimated_size() const { return estimated_size_; }

  bool CanRenderTo(const FeatureInfo* feature_info, GLint level) const;

  void SetServiceId(GLuint service_id) {
    DCHECK(service_id);
    DCHECK_EQ(owned_service_id_, service_id_);
    service_id_ = service_id;
    owned_service_id_ = service_id;
  }

  bool SafeToRenderFrom() const {
    return cleared_;
  }

  // Get the width/height/depth for a particular level. Returns false if level
  // does not exist.
  // |depth| is optional and can be nullptr.
  bool GetLevelSize(
      GLint target, GLint level,
      GLsizei* width, GLsizei* height, GLsizei* depth) const;

  // Get the type of a level. Returns false if level does not exist.
  bool GetLevelType(
      GLint target, GLint level, GLenum* type, GLenum* internal_format) const;

  // Set the image for a particular level. If a GLStreamTextureImage was
  // previously set with SetLevelStreamTextureImage(), this will reset
  // |service_id_| back to |owned_service_id_|, removing the service id override
  // set by the GLStreamTextureImage.
  void SetLevelImage(GLenum target,
                     GLint level,
                     gl::GLImage* image,
                     ImageState state);

  // Set the GLStreamTextureImage for a particular level.  This is like
  // SetLevelImage, but it also makes it optional to override |service_id_| with
  // a texture bound to the stream texture, and permits
  // GetLevelStreamTextureImage to return the image. See
  // SetStreamTextureServiceId() for the details of how |service_id| is used.
  void SetLevelStreamTextureImage(GLenum target,
                                  GLint level,
                                  GLStreamTextureImage* image,
                                  ImageState state,
                                  GLuint service_id);

  // Set the ImageState for the image bound to the given level.
  void SetLevelImageState(GLenum target, GLint level, ImageState state);


  // Get the image associated with a particular level. Returns NULL if level
  // does not exist.
  gl::GLImage* GetLevelImage(GLint target,
                             GLint level,
                             ImageState* state) const;
  gl::GLImage* GetLevelImage(GLint target, GLint level) const;

  // Like GetLevelImage, but will return NULL if the image wasn't set via
  // a call to SetLevelStreamTextureImage.
  GLStreamTextureImage* GetLevelStreamTextureImage(GLint target,
                                                   GLint level) const;

  bool HasImages() const {
    return has_images_;
  }

  // Returns true of the given dimensions are inside the dimensions of the
  // level.
  bool ValidForTexture(
      GLint target,
      GLint level,
      GLint xoffset,
      GLint yoffset,
      GLint zoffset,
      GLsizei width,
      GLsizei height,
      GLsizei depth) const;

  bool IsValid() const {
    return !!target();
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

  void SetImmutable(bool immutable);

  bool IsImmutable() const {
    return immutable_;
  }

  // Return 0 if it's not immutable.
  GLint GetImmutableLevels() const;

  // Get the cleared rectangle for a particular level. Returns an empty
  // rectangle if level does not exist.
  gfx::Rect GetLevelClearedRect(GLenum target, GLint level) const;

  // Whether a particular level/face is cleared.
  bool IsLevelCleared(GLenum target, GLint level) const;
  // Whether a particular level/face is partially cleared.
  bool IsLevelPartiallyCleared(GLenum target, GLint level) const;

  // Whether the texture has been defined
  bool IsDefined() const {
    return estimated_size() > 0;
  }

  // Initialize TEXTURE_MAX_ANISOTROPY to 1 if we haven't done so yet.
  void InitTextureMaxAnisotropyIfNeeded(GLenum target);

  void DumpLevelMemory(base::trace_event::ProcessMemoryDump* pmd,
                       uint64_t client_tracing_id,
                       const std::string& dump_name) const;

  void ApplyFormatWorkarounds(FeatureInfo* feature_info);

  bool EmulatingRGB();

  // In GLES2 "texture complete" means it has all required mips for filtering
  // down to a 1x1 pixel texture, they are in the correct order, they are all
  // the same format.
  bool texture_complete() const {
    DCHECK(!completeness_dirty_);
    return texture_complete_;
  }

  static bool ColorRenderable(const FeatureInfo* feature_info,
                              GLenum internal_format,
                              bool immutable);

 private:
  friend class MailboxManagerImpl;
  friend class MailboxManagerSync;
  friend class MailboxManagerTest;
  friend class TextureDefinition;
  friend class TextureManager;
  friend class TextureRef;
  friend class TextureTestHelper;

  ~Texture() override;
  void AddTextureRef(TextureRef* ref);
  void RemoveTextureRef(TextureRef* ref, bool have_context);
  MemoryTypeTracker* GetMemTracker();

  // Condition on which this texture is renderable. Can be ONLY_IF_NPOT if it
  // depends on context support for non-power-of-two textures (i.e. will be
  // renderable if NPOT support is in the context, otherwise not, e.g. texture
  // with a NPOT level). ALWAYS means it doesn't depend on context features
  // (e.g. complete POT), NEVER means it's not renderable regardless (e.g.
  // incomplete).
  enum CanRenderCondition {
    CAN_RENDER_ALWAYS,
    CAN_RENDER_NEVER,
    CAN_RENDER_NEEDS_VALIDATION,
  };

  struct LevelInfo {
    LevelInfo();
    LevelInfo(const LevelInfo& rhs);
    ~LevelInfo();

    gfx::Rect cleared_rect;
    GLenum target;
    GLint level;
    GLenum internal_format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum format;
    GLenum type;
    scoped_refptr<gl::GLImage> image;
    scoped_refptr<GLStreamTextureImage> stream_texture_image;
    ImageState image_state;
    uint32_t estimated_size;
    bool internal_workaround;
  };

  struct FaceInfo {
    FaceInfo();
    FaceInfo(const FaceInfo& other);
    ~FaceInfo();

    // This is relative to base_level and max_level of a texture.
    GLsizei num_mip_levels;
    // This contains slots for all levels starting at 0.
    std::vector<LevelInfo> level_infos;
  };

  // Helper for SetLevel*Image.  |stream_texture_image| may be null.
  void SetLevelImageInternal(GLenum target,
                             GLint level,
                             gl::GLImage* image,
                             GLStreamTextureImage* stream_texture_image,
                             ImageState state);

  // Returns the LevelInfo for |target| and |level| if it's set, else NULL.
  const LevelInfo* GetLevelInfo(GLint target, GLint level) const;

  // Set the info for a particular level.
  void SetLevelInfo(GLenum target,
                    GLint level,
                    GLenum internal_format,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const gfx::Rect& cleared_rect);

  // Causes us to report |service_id| as our service id, but does not delete
  // it when we are destroyed.  Will rebind any OES_EXTERNAL texture units to
  // our new service id in all contexts.  If |service_id| is zero, then we
  // revert to |owned_service_id_|.
  void SetStreamTextureServiceId(GLuint service_id);

  void MarkLevelAsInternalWorkaround(GLenum target, GLint level);

  // In GLES2 "cube complete" means all 6 faces level 0 are defined, all the
  // same format, all the same dimensions and all width = height.
  bool cube_complete() const {
    DCHECK(!completeness_dirty_);
    return cube_complete_;
  }

  // Whether or not this texture is a non-power-of-two texture.
  bool npot() const {
    return npot_;
  }

  // Marks a |rect| of a particular level as cleared.
  void SetLevelClearedRect(GLenum target,
                           GLint level,
                           const gfx::Rect& cleared_rect);

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
  // TODO(gman): Expand to SetParameteriv,fv
  // Returns GL_NO_ERROR on success. Otherwise the error to generate.
  GLenum SetParameteri(
      const FeatureInfo* feature_info, GLenum pname, GLint param);
  GLenum SetParameterf(
      const FeatureInfo* feature_info, GLenum pname, GLfloat param);

  // Makes each of the mip levels as though they were generated.
  void MarkMipmapsGenerated();

  bool NeedsMips() const {
    return sampler_state_.min_filter != GL_NEAREST &&
           sampler_state_.min_filter != GL_LINEAR;
  }

  // True if this texture meets all the GLES2 criteria for rendering.
  // See section 3.8.2 of the GLES2 spec.
  bool CanRender(const FeatureInfo* feature_info) const;
  bool CanRenderWithSampler(const FeatureInfo* feature_info,
                            const SamplerState& sampler_state) const;

  // Returns true if mipmaps can be generated by GL.
  bool CanGenerateMipmaps(const FeatureInfo* feature_info) const;

  // Returns true if any of the texture dimensions are not a power of two.
  static bool TextureIsNPOT(GLsizei width, GLsizei height, GLsizei depth);

  // Returns true if texture face is complete relative to the first face.
  static bool TextureFaceComplete(const Texture::LevelInfo& first_face,
                                  size_t face_index,
                                  GLenum target,
                                  GLenum internal_format,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type);

  // Returns true if texture mip level is complete relative to base level.
  // Note that level_diff = level - base_level.
  static bool TextureMipComplete(const Texture::LevelInfo& base_level_face,
                                 GLenum target,
                                 GLint level_diff,
                                 GLenum internal_format,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLenum format,
                                 GLenum type);

  static bool TextureFilterable(const FeatureInfo* feature_info,
                                GLenum internal_format,
                                GLenum type,
                                bool immutable);

  // Sets the Texture's target
  // Parameters:
  //   target: GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP or
  //           GL_TEXTURE_EXTERNAL_OES or GL_TEXTURE_RECTANGLE_ARB
  //           GL_TEXTURE_2D_ARRAY or GL_TEXTURE_3D (for GLES3)
  //   max_levels: The maximum levels this type of target can have.
  void SetTarget(GLenum target, GLint max_levels);

  // Update info about this texture.
  void Update();

  // Appends a signature for the given level.
  void AddToSignature(
      const FeatureInfo* feature_info,
      GLenum target, GLint level, std::string* signature) const;

  // Updates the unsafe textures count in all the managers referencing this
  // texture.
  void UpdateSafeToRenderFrom(bool cleared);

  // Updates the uncleared mip count in all the managers referencing this
  // texture.
  void UpdateMipCleared(LevelInfo* info,
                        GLsizei width,
                        GLsizei height,
                        const gfx::Rect& cleared_rect);

  // Computes the CanRenderCondition flag.
  CanRenderCondition GetCanRenderCondition() const;

  // Updates the unrenderable texture count in all the managers referencing this
  // texture.
  void UpdateCanRenderCondition();

  // Updates the images count in all the managers referencing this
  // texture.
  void UpdateHasImages();

  // Updates the flag that indicates whether this texture requires RGB
  // emulation.
  void UpdateEmulatingRGB();

  // Increment the framebuffer state change count in all the managers
  // referencing this texture.
  void IncAllFramebufferStateChangeCount();

  void UpdateBaseLevel(GLint base_level);
  void UpdateMaxLevel(GLint max_level);
  void UpdateNumMipLevels();

  // Increment the generation counter for all managers that have a reference to
  // this texture.
  void IncrementManagerServiceIdGeneration();

  // Return the service id of the texture that we will delete when we are
  // destroyed.
  GLuint owned_service_id() const { return owned_service_id_; }

  GLenum GetCompatibilitySwizzleForChannel(GLenum channel);
  void SetCompatibilitySwizzle(const CompatibilitySwizzle* swizzle);

  // Info about each face and level of texture.
  std::vector<FaceInfo> face_infos_;

  // The texture refs that point to this Texture.
  typedef std::set<TextureRef*> RefSet;
  RefSet refs_;

  // The single TextureRef that accounts for memory for this texture. Must be
  // one of refs_.
  TextureRef* memory_tracking_ref_;

  // The id of the texture that we are responsible for deleting.  Normally, this
  // is the same as |service_id_|, unless a GLStreamTextureImage with its own
  // service id is bound. In that case the GLStreamTextureImage service id is
  // stored in |service_id_| and overrides the owned service id for all purposes
  // except deleting the texture name.
  GLuint owned_service_id_;

  // Whether all renderable mips of this texture have been cleared.
  bool cleared_;

  int num_uncleared_mips_;
  int num_npot_faces_;

  // Texture parameters.
  SamplerState sampler_state_;
  GLenum usage_;
  GLint base_level_;
  GLint max_level_;
  GLenum swizzle_r_;
  GLenum swizzle_g_;
  GLenum swizzle_b_;
  GLenum swizzle_a_;

  // The maximum level that has been set.
  GLint max_level_set_;

  // Whether or not this texture is "texture complete"
  bool texture_complete_;

  // Whether or not this texture is "cube complete"
  bool cube_complete_;

  // Whether mip levels, base_level, or max_level have changed and
  // texture_completeness_ and cube_completeness_ should be reverified.
  bool completeness_dirty_;

  // Whether or not this texture is non-power-of-two
  bool npot_;

  // Whether this texture has ever been bound.
  bool has_been_bound_;

  // The number of framebuffers this texture is attached to.
  int framebuffer_attachment_count_;

  // Whether the texture is immutable and no further changes to the format
  // or dimensions of the texture object can be made.
  bool immutable_;

  // Whether or not this texture has images.
  bool has_images_;

  // Size in bytes this texture is assumed to take in memory.
  uint32_t estimated_size_;

  // Cache of the computed CanRenderCondition flag.
  CanRenderCondition can_render_condition_;

  // Whether we have initialized TEXTURE_MAX_ANISOTROPY to 1.
  bool texture_max_anisotropy_initialized_;

  const CompatibilitySwizzle* compatibility_swizzle_;

  bool emulating_rgb_;

  DISALLOW_COPY_AND_ASSIGN(Texture);
};

// This class represents a texture in a client context group. It's mostly 1:1
// with a client id, though it can outlive the client id if it's still bound to
// a FBO or another context when destroyed.
// Multiple TextureRef can point to the same texture with cross-context sharing.
class GPU_EXPORT TextureRef : public base::RefCounted<TextureRef> {
 public:
  TextureRef(TextureManager* manager, GLuint client_id, Texture* texture);
  static scoped_refptr<TextureRef> Create(TextureManager* manager,
                                          GLuint client_id,
                                          GLuint service_id);

  void AddObserver() { num_observers_++; }
  void RemoveObserver() { num_observers_--; }

  const Texture* texture() const { return texture_; }
  Texture* texture() { return texture_; }
  GLuint client_id() const { return client_id_; }
  GLuint service_id() const { return texture_->service_id(); }
  GLint num_observers() const { return num_observers_; }

  // When the TextureRef is destroyed, it will assume that the context has been
  // lost, regardless of the state of the TextureManager.
  void ForceContextLost();

 private:
  friend class base::RefCounted<TextureRef>;
  friend class Texture;
  friend class TextureManager;

  ~TextureRef();
  const TextureManager* manager() const { return manager_; }
  TextureManager* manager() { return manager_; }
  void reset_client_id() { client_id_ = 0; }

  TextureManager* manager_;
  Texture* texture_;
  GLuint client_id_;
  GLint num_observers_;
  bool force_context_lost_;

  DISALLOW_COPY_AND_ASSIGN(TextureRef);
};

// Holds data that is per gles2_cmd_decoder, but is related to to the
// TextureManager.
struct DecoderTextureState {
  // total_texture_upload_time automatically initialized to 0 in default
  // constructor.
  explicit DecoderTextureState(const GpuDriverBugWorkarounds& workarounds);

  // This indicates all the following texSubImage*D calls that are part of the
  // failed texImage*D call should be ignored. The client calls have a lock
  // around them, so it will affect only a single texImage*D + texSubImage*D
  // group.
  bool tex_image_failed;

  bool texsubimage_faster_than_teximage;
  bool force_cube_map_positive_x_allocation;
  bool force_cube_complete;
  bool force_int_or_srgb_cube_texture_complete;
  bool unpack_alignment_workaround_with_unpack_buffer;
  bool unpack_overlapping_rows_separately_unpack_buffer;
  bool unpack_image_height_workaround_with_unpack_buffer;
};

// This class keeps track of the textures and their sizes so we can do NPOT and
// texture complete checking.
//
// NOTE: To support shared resources an instance of this class will need to be
// shared by multiple GLES2Decoders.
class GPU_EXPORT TextureManager : public base::trace_event::MemoryDumpProvider {
 public:
  class GPU_EXPORT DestructionObserver {
   public:
    DestructionObserver();
    virtual ~DestructionObserver();

    // Called in ~TextureManager.
    virtual void OnTextureManagerDestroying(TextureManager* manager) = 0;

    // Called via ~TextureRef.
    virtual void OnTextureRefDestroying(TextureRef* texture) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
  };

  enum DefaultAndBlackTextures {
    kTexture2D,
    kTexture3D,
    kTexture2DArray,
    kCubeMap,
    kExternalOES,
    kRectangleARB,
    kNumDefaultTextures
  };

  TextureManager(MemoryTracker* memory_tracker,
                 FeatureInfo* feature_info,
                 GLsizei max_texture_size,
                 GLsizei max_cube_map_texture_size,
                 GLsizei max_rectangle_texture_size,
                 GLsizei max_3d_texture_size,
                 GLsizei max_array_texture_layers,
                 bool use_default_textures,
                 ProgressReporter* progress_reporter,
                 ServiceDiscardableManager* discardable_manager);
  ~TextureManager() override;

  void AddFramebufferManager(FramebufferManager* framebuffer_manager);
  void RemoveFramebufferManager(FramebufferManager* framebuffer_manager);

  // Init the texture manager.
  void Initialize();

  void MarkContextLost();

  // Must call before destruction.
  void Destroy();

  // Returns the maximum number of levels.
  GLint MaxLevelsForTarget(GLenum target) const {
    switch (target) {
      case GL_TEXTURE_2D:
      case GL_TEXTURE_2D_ARRAY:
        return max_levels_;
      case GL_TEXTURE_RECTANGLE_ARB:
      case GL_TEXTURE_EXTERNAL_OES:
        return 1;
      case GL_TEXTURE_3D:
        return max_3d_levels_;
      default:
        return max_cube_map_levels_;
    }
  }

  // Returns the maximum size.
  GLsizei MaxSizeForTarget(GLenum target) const {
    switch (target) {
      case GL_TEXTURE_2D:
      case GL_TEXTURE_EXTERNAL_OES:
      case GL_TEXTURE_2D_ARRAY:
        return max_texture_size_;
      case GL_TEXTURE_RECTANGLE:
        return max_rectangle_texture_size_;
      case GL_TEXTURE_3D:
        return max_3d_texture_size_;
      default:
        return max_cube_map_texture_size_;
    }
  }

  GLsizei max_array_texture_layers() const {
    return max_array_texture_layers_;
  }

  // Returns the maxium number of levels a texture of the given size can have.
  static GLsizei ComputeMipMapCount(GLenum target,
                                    GLsizei width,
                                    GLsizei height,
                                    GLsizei depth);

  static GLenum ExtractFormatFromStorageFormat(GLenum internalformat);
  static GLenum ExtractTypeFromStorageFormat(GLenum internalformat);

  // Checks if a dimensions are valid for a given target.
  bool ValidForTarget(
      GLenum target, GLint level,
      GLsizei width, GLsizei height, GLsizei depth);

  // True if this texture meets all the GLES2 criteria for rendering.
  // See section 3.8.2 of the GLES2 spec.
  bool CanRender(const TextureRef* ref) const {
    return ref->texture()->CanRender(feature_info_.get());
  }

  bool CanRenderWithSampler(
      const TextureRef* ref, const SamplerState& sampler_state) const {
    return ref->texture()->CanRenderWithSampler(
        feature_info_.get(), sampler_state);
  }

  // Returns true if mipmaps can be generated by GL.
  bool CanGenerateMipmaps(const TextureRef* ref) const {
    return ref->texture()->CanGenerateMipmaps(feature_info_.get());
  }

  // Sets the Texture's target
  // Parameters:
  //   target: GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP
  //           GL_TEXTURE_2D_ARRAY or GL_TEXTURE_3D (for GLES3)
  //   max_levels: The maximum levels this type of target can have.
  void SetTarget(
      TextureRef* ref,
      GLenum target);

  // Set the info for a particular level in a TexureInfo.
  void SetLevelInfo(TextureRef* ref,
                    GLenum target,
                    GLint level,
                    GLenum internal_format,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const gfx::Rect& cleared_rect);

  Texture* Produce(TextureRef* ref);

  // Maps an existing texture into the texture manager, at a given client ID.
  TextureRef* Consume(GLuint client_id, Texture* texture);

  // Sets |rect| of mip as cleared.
  void SetLevelClearedRect(TextureRef* ref,
                           GLenum target,
                           GLint level,
                           const gfx::Rect& cleared_rect);

  // Sets a mip as cleared.
  void SetLevelCleared(TextureRef* ref, GLenum target,
                       GLint level, bool cleared);

  // Sets a texture parameter of a Texture
  // Returns GL_NO_ERROR on success. Otherwise the error to generate.
  // TODO(gman): Expand to SetParameteriv,fv
  void SetParameteri(
      const char* function_name, ErrorState* error_state,
      TextureRef* ref, GLenum pname, GLint param);
  void SetParameterf(
      const char* function_name, ErrorState* error_state,
      TextureRef* ref, GLenum pname, GLfloat param);

  // Makes each of the mip levels as though they were generated.
  void MarkMipmapsGenerated(TextureRef* ref);

  // Clears any uncleared renderable levels.
  bool ClearRenderableLevels(GLES2Decoder* decoder, TextureRef* ref);

  // Clear a specific level.
  bool ClearTextureLevel(
      GLES2Decoder* decoder, TextureRef* ref, GLenum target, GLint level);

  // Creates a new texture info.
  TextureRef* CreateTexture(GLuint client_id, GLuint service_id);

  // Gets the texture info for the given texture.
  TextureRef* GetTexture(GLuint client_id) const;

  // Takes the TextureRef for the given texture out of the texture manager.
  scoped_refptr<TextureRef> TakeTexture(GLuint client_id);

  // Returns a TextureRef to the texture manager.
  void ReturnTexture(scoped_refptr<TextureRef> texture_ref);

  // Removes a texture info.
  void RemoveTexture(GLuint client_id);

  // Gets a Texture for a given service id (note: it assumes the texture object
  // is still mapped in this TextureManager).
  Texture* GetTextureForServiceId(GLuint service_id) const;

  TextureRef* GetDefaultTextureInfo(GLenum target) {
    switch (target) {
      case GL_TEXTURE_2D:
        return default_textures_[kTexture2D].get();
      case GL_TEXTURE_3D:
        return default_textures_[kTexture3D].get();
      case GL_TEXTURE_2D_ARRAY:
        return default_textures_[kTexture2DArray].get();
      case GL_TEXTURE_CUBE_MAP:
        return default_textures_[kCubeMap].get();
      case GL_TEXTURE_EXTERNAL_OES:
        return default_textures_[kExternalOES].get();
      case GL_TEXTURE_RECTANGLE_ARB:
        return default_textures_[kRectangleARB].get();
      default:
        NOTREACHED();
        return NULL;
    }
  }

  bool HaveUnsafeTextures() const {
    return num_unsafe_textures_ > 0;
  }

  bool HaveUnclearedMips() const {
    return num_uncleared_mips_ > 0;
  }

  bool HaveImages() const {
    return num_images_ > 0;
  }

  GLuint black_texture_id(GLenum target) const {
    switch (target) {
      case GL_SAMPLER_2D:
        return black_texture_ids_[kTexture2D];
      case GL_SAMPLER_3D:
        return black_texture_ids_[kTexture3D];
      case GL_SAMPLER_2D_ARRAY:
        return black_texture_ids_[kTexture2DArray];
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
    return memory_type_tracker_->GetMemRepresented();
  }

  void SetLevelImage(TextureRef* ref,
                     GLenum target,
                     GLint level,
                     gl::GLImage* image,
                     Texture::ImageState state);

  void SetLevelStreamTextureImage(TextureRef* ref,
                                  GLenum target,
                                  GLint level,
                                  GLStreamTextureImage* image,
                                  Texture::ImageState state,
                                  GLuint service_id);

  void SetLevelImageState(TextureRef* ref,
                          GLenum target,
                          GLint level,
                          Texture::ImageState state);

  size_t GetSignatureSize() const;

  void AddToSignature(
      TextureRef* ref,
      GLenum target,
      GLint level,
      std::string* signature) const;

  void AddObserver(DestructionObserver* observer) {
    destruction_observers_.push_back(observer);
  }

  void RemoveObserver(DestructionObserver* observer) {
    for (unsigned int i = 0; i < destruction_observers_.size(); i++) {
      if (destruction_observers_[i] == observer) {
        std::swap(destruction_observers_[i], destruction_observers_.back());
        destruction_observers_.pop_back();
        return;
      }
    }
    NOTREACHED();
  }

  struct DoTexImageArguments {
    enum TexImageCommandType {
      kTexImage2D,
      kTexImage3D,
    };

    GLenum target;
    GLint level;
    GLenum internal_format;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLint border;
    GLenum format;
    GLenum type;
    const void* pixels;
    uint32_t pixels_size;
    uint32_t padding;
    TexImageCommandType command_type;
  };

  bool ValidateTexImage(
    ContextState* state,
    const char* function_name,
    const DoTexImageArguments& args,
    // Pointer to TextureRef filled in if validation successful.
    // Presumes the pointer is valid.
    TextureRef** texture_ref);

  void ValidateAndDoTexImage(
    DecoderTextureState* texture_state,
    ContextState* state,
    DecoderFramebufferState* framebuffer_state,
    const char* function_name,
    const DoTexImageArguments& args);

  struct DoTexSubImageArguments {
    enum TexSubImageCommandType {
      kTexSubImage2D,
      kTexSubImage3D,
    };

    GLenum target;
    GLint level;
    GLint xoffset;
    GLint yoffset;
    GLint zoffset;
    GLsizei width;
    GLsizei height;
    GLsizei depth;
    GLenum format;
    GLenum type;
    const void* pixels;
    uint32_t pixels_size;
    uint32_t padding;
    TexSubImageCommandType command_type;
  };

  bool ValidateTexSubImage(
      ContextState* state,
      const char* function_name,
      const DoTexSubImageArguments& args,
      // Pointer to TextureRef filled in if validation successful.
      // Presumes the pointer is valid.
      TextureRef** texture_ref);

  void ValidateAndDoTexSubImage(GLES2Decoder* decoder,
                                DecoderTextureState* texture_state,
                                ContextState* state,
                                DecoderFramebufferState* framebuffer_state,
                                const char* function_name,
                                const DoTexSubImageArguments& args);

  // TODO(kloveless): Make GetTexture* private once this is no longer called
  // from gles2_cmd_decoder.
  TextureRef* GetTextureInfoForTarget(ContextState* state, GLenum target);
  TextureRef* GetTextureInfoForTargetUnlessDefault(
      ContextState* state, GLenum target);

  // This function is used to validate TexImage2D and TexSubImage2D and their
  // variants. But internal_format only checked for callers of TexImage2D and
  // its variants (tex_image_call is true).
  bool ValidateTextureParameters(
    ErrorState* error_state, const char* function_name, bool tex_image_call,
    GLenum format, GLenum type, GLint internal_format, GLint level);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Returns the union of |rect1| and |rect2| if one of the rectangles is empty,
  // contains the other rectangle or shares an edge with the other rectangle.
  // Part of the public interface because texture pixel data rectangle
  // operations are also implemented in decoder at the moment.
  static bool CombineAdjacentRects(const gfx::Rect& rect1,
                                   const gfx::Rect& rect2,
                                   gfx::Rect* result);

  // Get / set the current generation number of this manager.  This generation
  // number changes whenever the service_id of one or more Textures change.
  uint32_t GetServiceIdGeneration() const;
  void IncrementServiceIdGeneration();

  static GLenum AdjustTexInternalFormat(const gles2::FeatureInfo* feature_info,
                                        GLenum format);
  static GLenum AdjustTexFormat(const gles2::FeatureInfo* feature_info,
                                GLenum format);

  void WorkaroundCopyTexImageCubeMap(
      DecoderTextureState* texture_state,
      ContextState* state,
      DecoderFramebufferState* framebuffer_state,
      TextureRef* texture_ref,
      const char* function_name,
      const DoTexImageArguments& args) {
    DoCubeMapWorkaround(texture_state, state, framebuffer_state,
                        texture_ref, function_name, args);
  }

 private:
  friend class Texture;
  friend class TextureRef;

  // Helper for Initialize().
  scoped_refptr<TextureRef> CreateDefaultAndBlackTextures(
      GLenum target,
      GLuint* black_texture);

  void DoTexImage(
      DecoderTextureState* texture_state,
      ContextState* state,
      DecoderFramebufferState* framebuffer_state,
      const char* function_name,
      TextureRef* texture_ref,
      const DoTexImageArguments& args);

  // Reserve memory for the texture and set its attributes so it can be filled
  // with TexSubImage. The image contents are undefined after this function,
  // so make sure it's subsequently filled in its entirety.
  void ReserveTexImageToBeFilled(DecoderTextureState* texture_state,
                                 ContextState* state,
                                 DecoderFramebufferState* framebuffer_state,
                                 const char* function_name,
                                 TextureRef* texture_ref,
                                 const DoTexImageArguments& args);

  void DoTexSubImageWithAlignmentWorkaround(
      DecoderTextureState* texture_state,
      ContextState* state,
      const DoTexSubImageArguments& args);

  void DoTexSubImageRowByRowWorkaround(DecoderTextureState* texture_state,
                                       ContextState* state,
                                       const DoTexSubImageArguments& args,
                                       const PixelStoreParams& unpack_params);

  void DoTexSubImageLayerByLayerWorkaround(
      DecoderTextureState* texture_state,
      ContextState* state,
      const DoTexSubImageArguments& args,
      const PixelStoreParams& unpack_params);

  void DoCubeMapWorkaround(
      DecoderTextureState* texture_state,
      ContextState* state,
      DecoderFramebufferState* framebuffer_state,
      TextureRef* texture_ref,
      const char* function_name,
      const DoTexImageArguments& args);

  void StartTracking(TextureRef* texture);
  void StopTracking(TextureRef* texture);

  void UpdateSafeToRenderFrom(int delta);
  void UpdateUnclearedMips(int delta);
  void UpdateCanRenderCondition(Texture::CanRenderCondition old_condition,
                                Texture::CanRenderCondition new_condition);
  void UpdateNumImages(int delta);
  void IncFramebufferStateChangeCount();

  // Helper function called by OnMemoryDump.
  void DumpTextureRef(base::trace_event::ProcessMemoryDump* pmd,
                      TextureRef* ref);

  MemoryTypeTracker* GetMemTracker();
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  MemoryTracker* memory_tracker_;

  scoped_refptr<FeatureInfo> feature_info_;

  std::vector<FramebufferManager*> framebuffer_managers_;

  // Info for each texture in the system.
  typedef base::hash_map<GLuint, scoped_refptr<TextureRef> > TextureMap;
  TextureMap textures_;

  GLsizei max_texture_size_;
  GLsizei max_cube_map_texture_size_;
  GLsizei max_rectangle_texture_size_;
  GLsizei max_3d_texture_size_;
  GLsizei max_array_texture_layers_;
  GLint max_levels_;
  GLint max_cube_map_levels_;
  GLint max_3d_levels_;

  const bool use_default_textures_;

  int num_unsafe_textures_;
  int num_uncleared_mips_;
  int num_images_;

  // Counts the number of Textures allocated with 'this' as its manager.
  // Allows to check no Texture will outlive this.
  unsigned int texture_count_;

  bool have_context_;

  // Black (0,0,0,1) textures for when non-renderable textures are used.
  // NOTE: There is no corresponding Texture for these textures.
  // TextureInfos are only for textures the client side can access.
  GLuint black_texture_ids_[kNumDefaultTextures];

  // The default textures for each target (texture name = 0)
  scoped_refptr<TextureRef> default_textures_[kNumDefaultTextures];

  std::vector<DestructionObserver*> destruction_observers_;

  uint32_t current_service_id_generation_;

  // Used to notify the watchdog thread of progress during destruction,
  // preventing time-outs when destruction takes a long time. May be null when
  // using in-process command buffer.
  ProgressReporter* progress_reporter_;

  ServiceDiscardableManager* discardable_manager_;

  DISALLOW_COPY_AND_ASSIGN(TextureManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_MANAGER_H_
