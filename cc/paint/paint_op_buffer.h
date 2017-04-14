// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_OP_BUFFER_H_
#define CC_PAINT_PAINT_OP_BUFFER_H_

#include <stdint.h>

#include "base/logging.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkTextBlob.h"

// PaintOpBuffer is a reimplementation of SkLiteDL.
// See: third_party/skia/src/core/SkLiteDL.h.

namespace cc {

class DisplayItemList;

class ThreadsafeMatrix : public SkMatrix {
 public:
  explicit ThreadsafeMatrix(const SkMatrix& matrix) : SkMatrix(matrix) {
    (void)getType();
  }
};

class ThreadsafePath : public SkPath {
 public:
  explicit ThreadsafePath(const SkPath& path) : SkPath(path) {
    updateBoundsCache();
  }
};

enum class PaintOpType : uint8_t {
  Annotate,
  ClipPath,
  ClipRect,
  ClipRRect,
  Concat,
  DrawArc,
  DrawCircle,
  DrawColor,
  DrawDisplayItemList,
  DrawDRRect,
  DrawImage,
  DrawImageRect,
  DrawIRect,
  DrawLine,
  DrawOval,
  DrawPath,
  DrawPosText,
  DrawRecord,
  DrawRect,
  DrawRRect,
  DrawText,
  DrawTextBlob,
  Noop,
  Restore,
  Rotate,
  Save,
  SaveLayer,
  SaveLayerAlpha,
  Scale,
  SetMatrix,
  Translate,
  LastPaintOpType = Translate,
};

struct CC_PAINT_EXPORT PaintOp {
  uint32_t type : 8;
  uint32_t skip : 24;

  PaintOpType GetType() const { return static_cast<PaintOpType>(type); }

  void Raster(SkCanvas* canvas, const SkMatrix& original_ctm) const;
  bool IsDrawOp() const;

  // Only valid for draw ops.
  void RasterWithAlpha(SkCanvas* canvas, uint8_t alpha) const;

  int CountSlowPaths() const { return 0; }

  // Returns the number of bytes used by this op in referenced sub records
  // and display lists.  This doesn't count other objects like paths or blobs.
  size_t AdditionalBytesUsed() const { return 0; }

  static constexpr bool kIsDrawOp = false;
  // If an op has |kHasPaintFlags| set to true, it must:
  // (1) Provide a PaintFlags member called |flags|
  // (2) Provide a RasterWithFlags function instead of a Raster function.
  static constexpr bool kHasPaintFlags = false;
  static SkRect kUnsetRect;
};

struct PaintOpWithData : PaintOp {
  // Having data is just a helper for ops that have a varying amount of data and
  // want a way to store that inline.  This is for ops that pass in a
  // void* and a length.
  explicit PaintOpWithData(size_t bytes) : bytes(bytes) {}

  // Get data out by calling paint_op_data.  This can't be part of the class
  // because it needs to know the size of the derived type.
  size_t bytes;
};

template <typename T>
const void* paint_op_data(const T* op) {
  static_assert(std::is_convertible<T, PaintOpWithData>::value,
                "T is not a PaintOpWithData");
  // Arbitrary data for a PaintOp is stored after the PaintOp itself
  // in the PaintOpBuffer.  Therefore, to access this data, it's
  // pointer math to increment past the size of T.  Accessing the
  // next op in the buffer is ((char*)op) + op->skip, with the data
  // fitting between.
  return op + 1;
}

template <typename T>
void* paint_op_data(T* op) {
  static_assert(std::is_convertible<T, PaintOpWithData>::value,
                "T is not a PaintOpWithData");
  return op + 1;
}

struct PaintOpWithDataArrayBase : PaintOpWithData {
  // Helper class for static asserts in push functions.
  using PaintOpWithData::PaintOpWithData;
};

template <typename T>
struct PaintOpWithDataArray : PaintOpWithDataArrayBase {
  // Paint op that has a T[count] and a char[bytes].
  PaintOpWithDataArray(size_t bytes, size_t count)
      : PaintOpWithDataArrayBase(bytes), count(count) {}
  // Use paint_op_array to get array data.

  size_t count;
};

template <typename M, typename T>
const M* paint_op_array(const T* op) {
  static_assert(std::is_convertible<T, PaintOpWithDataArrayBase>::value,
                "T is not a PaintOpWithDataArray");
  // See comment in paint_op_data.  Array data is stored after
  // any void* data.  Memory layout here is: |op|data|array data|next op|
  return SkTAddOffset<const M>(op + 1, op->bytes);
}
template <typename M, typename T>
M* paint_op_array(T* op) {
  static_assert(std::is_convertible<T, PaintOpWithDataArrayBase>::value,
                "T is not a PaintOpWithDataArray");
  return SkTAddOffset<M>(op + 1, op->bytes);
}

struct AnnotateOp final : PaintOp {
  enum class AnnotationType {
    URL,
    LinkToDestination,
    NamedDestination,
  };

  static constexpr PaintOpType kType = PaintOpType::Annotate;
  AnnotateOp(PaintCanvas::AnnotationType annotation_type,
             const SkRect& rect,
             sk_sp<SkData> data);
  ~AnnotateOp();
  void Raster(SkCanvas* canvas) const;

  PaintCanvas::AnnotationType annotation_type;
  SkRect rect;
  sk_sp<SkData> data;
};

struct ClipPathOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::ClipPath;
  ClipPathOp(SkPath path, SkClipOp op, bool antialias)
      : path(path), op(op), antialias(antialias) {}
  void Raster(SkCanvas* canvas) const;
  int CountSlowPaths() const;

  ThreadsafePath path;
  SkClipOp op;
  bool antialias;
};

struct ClipRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::ClipRect;
  ClipRectOp(const SkRect& rect, SkClipOp op, bool antialias)
      : rect(rect), op(op), antialias(antialias) {}
  void Raster(SkCanvas* canvas) const;

  SkRect rect;
  SkClipOp op;
  bool antialias;
};

struct ClipRRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::ClipRRect;
  ClipRRectOp(const SkRRect& rrect, SkClipOp op, bool antialias)
      : rrect(rrect), op(op), antialias(antialias) {}
  void Raster(SkCanvas* canvas) const;

  SkRRect rrect;
  SkClipOp op;
  bool antialias;
};

struct ConcatOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Concat;
  explicit ConcatOp(const SkMatrix& matrix) : matrix(matrix) {}
  void Raster(SkCanvas* canvas) const;

  ThreadsafeMatrix matrix;
};

struct DrawArcOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawArc;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawArcOp(const SkRect& oval,
            SkScalar start_angle,
            SkScalar sweep_angle,
            bool use_center,
            const PaintFlags& flags)
      : oval(oval),
        start_angle(start_angle),
        sweep_angle(sweep_angle),
        use_center(use_center),
        flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkRect oval;
  SkScalar start_angle;
  SkScalar sweep_angle;
  bool use_center;
  PaintFlags flags;
};

struct DrawCircleOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawCircle;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawCircleOp(SkScalar cx,
               SkScalar cy,
               SkScalar radius,
               const PaintFlags& flags)
      : cx(cx), cy(cy), radius(radius), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkScalar cx;
  SkScalar cy;
  SkScalar radius;
  PaintFlags flags;
};

struct DrawColorOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawColor;
  static constexpr bool kIsDrawOp = true;
  DrawColorOp(SkColor color, SkBlendMode mode) : color(color), mode(mode) {}
  void Raster(SkCanvas* canvas) const;

  SkColor color;
  SkBlendMode mode;
};

struct DrawDisplayItemListOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawDisplayItemList;
  static constexpr bool kIsDrawOp = true;
  explicit DrawDisplayItemListOp(scoped_refptr<DisplayItemList> list);
  ~DrawDisplayItemListOp();
  void Raster(SkCanvas* canvas) const;
  size_t AdditionalBytesUsed() const;
  // TODO(enne): DisplayItemList should know number of slow paths.

  scoped_refptr<DisplayItemList> list;
};

struct DrawDRRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawDRRect;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawDRRectOp(const SkRRect& outer,
               const SkRRect& inner,
               const PaintFlags& flags)
      : outer(outer), inner(inner), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkRRect outer;
  SkRRect inner;
  PaintFlags flags;
};

struct DrawImageOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawImage;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawImageOp(const PaintImage& image,
              SkScalar left,
              SkScalar top,
              const PaintFlags* flags);
  ~DrawImageOp();
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  PaintImage image;
  SkScalar left;
  SkScalar top;
  PaintFlags flags;
};

struct DrawImageRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawImageRect;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawImageRectOp(const PaintImage& image,
                  const SkRect& src,
                  const SkRect& dst,
                  const PaintFlags* flags,
                  PaintCanvas::SrcRectConstraint constraint);
  ~DrawImageRectOp();
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  PaintImage image;
  PaintFlags flags;
  SkRect src;
  SkRect dst;
  PaintCanvas::SrcRectConstraint constraint;
};

struct DrawIRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawIRect;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawIRectOp(const SkIRect& rect, const PaintFlags& flags)
      : rect(rect), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkIRect rect;
  PaintFlags flags;
};

struct DrawLineOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawLine;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawLineOp(SkScalar x0,
             SkScalar y0,
             SkScalar x1,
             SkScalar y1,
             const PaintFlags& flags)
      : x0(x0), y0(y0), x1(x1), y1(y1), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;
  int CountSlowPaths() const;

  SkScalar x0;
  SkScalar y0;
  SkScalar x1;
  SkScalar y1;
  PaintFlags flags;
};

struct DrawOvalOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawOval;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawOvalOp(const SkRect& oval, const PaintFlags& flags)
      : oval(oval), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkRect oval;
  PaintFlags flags;
};

struct DrawPathOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawPath;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawPathOp(const SkPath& path, const PaintFlags& flags)
      : path(path), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;
  int CountSlowPaths() const;

  ThreadsafePath path;
  PaintFlags flags;
};

struct DrawPosTextOp final : PaintOpWithDataArray<SkPoint> {
  static constexpr PaintOpType kType = PaintOpType::DrawPosText;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawPosTextOp(size_t bytes, size_t count, const PaintFlags& flags);
  ~DrawPosTextOp();
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  PaintFlags flags;
};

struct DrawRecordOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawRecord;
  static constexpr bool kIsDrawOp = true;
  explicit DrawRecordOp(sk_sp<const PaintRecord> record);
  ~DrawRecordOp();
  void Raster(SkCanvas* canvas) const;
  size_t AdditionalBytesUsed() const;

  sk_sp<const PaintRecord> record;
};

struct DrawRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawRect;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawRectOp(const SkRect& rect, const PaintFlags& flags)
      : rect(rect), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkRect rect;
  PaintFlags flags;
};

struct DrawRRectOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawRRect;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawRRectOp(const SkRRect& rrect, const PaintFlags& flags)
      : rrect(rrect), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkRRect rrect;
  PaintFlags flags;
};

struct DrawTextOp final : PaintOpWithData {
  static constexpr PaintOpType kType = PaintOpType::DrawText;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawTextOp(size_t bytes, SkScalar x, SkScalar y, const PaintFlags& flags)
      : PaintOpWithData(bytes), x(x), y(y), flags(flags) {}
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkScalar x;
  SkScalar y;
  PaintFlags flags;
};

struct DrawTextBlobOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::DrawTextBlob;
  static constexpr bool kIsDrawOp = true;
  static constexpr bool kHasPaintFlags = true;
  DrawTextBlobOp(sk_sp<SkTextBlob> blob,
                 SkScalar x,
                 SkScalar y,
                 const PaintFlags& flags);
  ~DrawTextBlobOp();
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  sk_sp<SkTextBlob> blob;
  SkScalar x;
  SkScalar y;
  PaintFlags flags;
};

struct NoopOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Noop;
  void Raster(SkCanvas* canvas) const {}
};

struct RestoreOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Restore;
  void Raster(SkCanvas* canvas) const;
};

struct RotateOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Rotate;
  explicit RotateOp(SkScalar degrees) : degrees(degrees) {}
  void Raster(SkCanvas* canvas) const;

  SkScalar degrees;
};

struct SaveOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Save;
  void Raster(SkCanvas* canvas) const;
};

struct SaveLayerOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::SaveLayer;
  static constexpr bool kHasPaintFlags = true;
  SaveLayerOp(const SkRect* bounds, const PaintFlags* flags)
      : bounds(bounds ? *bounds : kUnsetRect) {
    if (flags)
      this->flags = *flags;
  }
  void RasterWithFlags(SkCanvas* canvas, const PaintFlags& flags) const;

  SkRect bounds;
  PaintFlags flags;
};

struct SaveLayerAlphaOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::SaveLayerAlpha;
  SaveLayerAlphaOp(const SkRect* bounds, uint8_t alpha)
      : bounds(bounds ? *bounds : kUnsetRect), alpha(alpha) {}
  void Raster(SkCanvas* canvas) const;

  SkRect bounds;
  uint8_t alpha;
};

struct ScaleOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Scale;
  ScaleOp(SkScalar sx, SkScalar sy) : sx(sx), sy(sy) {}
  void Raster(SkCanvas* canvas) const;

  SkScalar sx;
  SkScalar sy;
};

struct SetMatrixOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::SetMatrix;
  explicit SetMatrixOp(const SkMatrix& matrix) : matrix(matrix) {}
  // This is the only op that needs the original ctm of the SkCanvas
  // used for raster (since SetMatrix is relative to the recording origin and
  // shouldn't clobber the SkCanvas raster origin).
  //
  // TODO(enne): Find some cleaner way to do this, possibly by making
  // all SetMatrix calls Concat??
  void Raster(SkCanvas* canvas, const SkMatrix& original_ctm) const;

  ThreadsafeMatrix matrix;
};

struct TranslateOp final : PaintOp {
  static constexpr PaintOpType kType = PaintOpType::Translate;
  TranslateOp(SkScalar dx, SkScalar dy) : dx(dx), dy(dy) {}
  void Raster(SkCanvas* canvas) const;

  SkScalar dx;
  SkScalar dy;
};

using LargestPaintOp = DrawDRRectOp;

class CC_PAINT_EXPORT PaintOpBuffer : public SkRefCnt {
 public:
  enum { kInitialBufferSize = 4096 };

  PaintOpBuffer();
  explicit PaintOpBuffer(const SkRect& cull_rect);
  ~PaintOpBuffer() override;

  void Reset();

  void playback(SkCanvas* canvas) const;
  void playback(SkCanvas* canvas, SkPicture::AbortCallback* callback) const;

  // TODO(enne): These are no longer approximate.  Rename these.
  int approximateOpCount() const { return op_count_; }
  size_t approximateBytesUsed() const {
    return sizeof(*this) + reserved_ + subrecord_bytes_used_;
  }
  int numSlowPaths() const { return num_slow_paths_; }

  // Resize the PaintOpBuffer to exactly fit the current amount of used space.
  void ShrinkToFit();

  const SkRect& cullRect() const { return cull_rect_; }

  PaintOp* GetFirstOp() const {
    return reinterpret_cast<PaintOp*>(const_cast<char*>(&first_op_[0]));
  }

  template <typename T, typename... Args>
  void push(Args&&... args) {
    static_assert(std::is_convertible<T, PaintOp>::value, "T not a PaintOp.");
    static_assert(!std::is_convertible<T, PaintOpWithData>::value,
                  "Type needs to use push_with_data");
    push_internal<T>(0, std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  void push_with_data(const void* data, size_t bytes, Args&&... args) {
    static_assert(std::is_convertible<T, PaintOpWithData>::value,
                  "T is not a PaintOpWithData");
#if !defined(OS_CHROMEOS)
    // TODO(enne): non-linux chromeos builds think that DrawTextOp
    // can be converted to a PaintOpWithDataArrayBase.  OOPS.
    static_assert(!std::is_convertible<T, PaintOpWithDataArrayBase>::value,
                  "Type needs to use push_with_data_array");
#endif
    DCHECK_GE(bytes, 0u);
    T* op = push_internal<T>(bytes, bytes, std::forward<Args>(args)...);
    memcpy(paint_op_data(op), data, bytes);

#if DCHECK_IS_ON()
    // Double check the data fits between op and next op and doesn't clobber.
    char* op_start = reinterpret_cast<char*>(op);
    char* op_end = op_start + sizeof(T);
    char* next_op = op_start + op->skip;
    char* data_start = reinterpret_cast<char*>(paint_op_data(op));
    char* data_end = data_start + bytes;
    DCHECK_GE(data_start, op_end);
    DCHECK_LT(data_start, next_op);
    DCHECK_LE(data_end, next_op);
#endif
  }

  template <typename T, typename M, typename... Args>
  void push_with_data_array(const void* data,
                            size_t bytes,
                            const M* array,
                            size_t count,
                            Args&&... args) {
    static_assert(std::is_convertible<T, PaintOpWithDataArray<M>>::value,
                  "T is not a PaintOpWithDataArray");
    DCHECK_GE(bytes, 0u);
    DCHECK_GE(count, 0u);
    size_t array_size = sizeof(M) * count;
    size_t total_size = bytes + array_size;
    T* op =
        push_internal<T>(total_size, bytes, count, std::forward<Args>(args)...);
    memcpy(paint_op_data(op), data, bytes);
    memcpy(paint_op_array<M>(op), array, array_size);

#if DCHECK_IS_ON()
    // Double check data and array don't clobber op, next op, or each other
    char* op_start = reinterpret_cast<char*>(op);
    char* op_end = op_start + sizeof(T);
    char* next_op = op_start + op->skip;
    char* data_start = reinterpret_cast<char*>(paint_op_data(op));
    char* data_end = data_start + bytes;
    char* array_start = reinterpret_cast<char*>(paint_op_array<M>(op));
    char* array_end = array_start + array_size;
    DCHECK_GE(data_start, op_end);
    DCHECK_LE(data_start, array_start);
    DCHECK_GE(array_start, data_end);
    DCHECK_LE(array_end, next_op);
#endif
  }

  class Iterator {
   public:
    explicit Iterator(const PaintOpBuffer* buffer)
        : buffer_(buffer), ptr_(buffer_->data_.get()) {}

    PaintOp* operator->() const {
      return op_idx_ ? reinterpret_cast<PaintOp*>(ptr_) : buffer_->GetFirstOp();
    }
    PaintOp* operator*() const { return operator->(); }
    Iterator begin() { return Iterator(buffer_, buffer_->data_.get(), 0); }
    Iterator end() {
      return Iterator(buffer_, buffer_->data_.get() + buffer_->used_,
                      buffer_->approximateOpCount());
    }
    bool operator!=(const Iterator& other) {
      // Not valid to compare iterators on different buffers.
      DCHECK_EQ(other.buffer_, buffer_);
      return other.op_idx_ != op_idx_;
    }
    Iterator& operator++() {
      if (!op_idx_++)
        return *this;
      PaintOp* op = **this;
      uint32_t type = op->type;
      CHECK_LE(type, static_cast<uint32_t>(PaintOpType::LastPaintOpType));
      ptr_ += op->skip;
      return *this;
    }
    operator bool() const { return op_idx_ < buffer_->approximateOpCount(); }

    int op_idx() const { return op_idx_; }

    // Return the next op without advancing the iterator, or nullptr if none.
    PaintOp* peek1() const {
      if (op_idx_ + 1 >= buffer_->approximateOpCount())
        return nullptr;
      if (!op_idx_)
        return reinterpret_cast<PaintOp*>(ptr_);
      return reinterpret_cast<PaintOp*>(ptr_ + (*this)->skip);
    }

    // Return the op two ops ahead without advancing the iterator, or nullptr if
    // none.
    PaintOp* peek2() const {
      if (op_idx_ + 2 >= buffer_->approximateOpCount())
        return nullptr;
      char* next = ptr_ + reinterpret_cast<PaintOp*>(ptr_)->skip;
      PaintOp* next_op = reinterpret_cast<PaintOp*>(next);
      if (!op_idx_)
        return next_op;
      return reinterpret_cast<PaintOp*>(next + next_op->skip);
    }

   private:
    Iterator(const PaintOpBuffer* buffer, char* ptr, int op_idx)
        : buffer_(buffer), ptr_(ptr), op_idx_(op_idx) {}

    const PaintOpBuffer* buffer_ = nullptr;
    char* ptr_ = nullptr;
    int op_idx_ = 0;
  };

 private:
  template <typename T, bool HasFlags>
  struct CountSlowPathsFromFlags {
    static int Count(const T* op) { return 0; }
  };

  template <typename T>
  struct CountSlowPathsFromFlags<T, true> {
    static int Count(const T* op) { return op->flags.getPathEffect() ? 1 : 0; }
  };

  template <typename T, typename... Args>
  T* push_internal(size_t bytes, Args&&... args) {
    size_t skip = SkAlignPtr(sizeof(T) + bytes);
    DCHECK_LT(skip, static_cast<size_t>(1) << 24);
    if (used_ + skip > reserved_ || !op_count_) {
      if (!op_count_) {
        if (bytes) {
          // Internal first_op buffer doesn't have room for extra data.
          // If the op wants extra bytes, then we'll just store a Noop
          // in the first_op and proceed from there.  This seems unlikely
          // to be a common case.
          push<NoopOp>();
        } else {
          T* op = reinterpret_cast<T*>(&first_op_[0]);
          new (op) T{std::forward<Args>(args)...};
          op->type = static_cast<uint32_t>(T::kType);
          op->skip = 0;
          op_count_++;
          return op;
        }
      }

      // Start reserved_ at kInitialBufferSize and then double.
      // ShrinkToFit can make this smaller afterwards.
      while (used_ + skip > reserved_)
        reserved_ = reserved_ ? reserved_ * 2 : kInitialBufferSize;
      data_.realloc(reserved_);
    }
    DCHECK_LE(used_ + skip, reserved_);

    T* op = reinterpret_cast<T*>(data_.get() + used_);
    used_ += skip;
    new (op) T(std::forward<Args>(args)...);
    op->type = static_cast<uint32_t>(T::kType);
    op->skip = skip;
    op_count_++;

    num_slow_paths_ += CountSlowPathsFromFlags<T, T::kHasPaintFlags>::Count(op);
    num_slow_paths_ += op->CountSlowPaths();

    subrecord_bytes_used_ += op->AdditionalBytesUsed();

    return op;
  }

  // As a performance optimization because n=1 is an extremely common case just
  // store the first op in the PaintOpBuffer itself to avoid an extra alloc.
  char first_op_[sizeof(LargestPaintOp)];
  SkAutoTMalloc<char> data_;
  size_t used_ = 0;
  size_t reserved_ = 0;
  int op_count_ = 0;

  // Record paths for veto-to-msaa for gpu raster.
  int num_slow_paths_ = 0;
  // Record additional bytes used by referenced sub-records and display lists.
  size_t subrecord_bytes_used_ = 0;
  SkRect cull_rect_;

  DISALLOW_COPY_AND_ASSIGN(PaintOpBuffer);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_OP_BUFFER_H_
