// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_ZUCCHINI_IMAGE_INDEX_H_
#define CHROME_INSTALLER_ZUCCHINI_IMAGE_INDEX_H_

#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "base/optional.h"
#include "chrome/installer/zucchini/buffer_view.h"
#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/label_manager.h"

namespace zucchini {

// A class that holds annotations of an image, allowing quick access to its
// raw and reference content. The memory overhead of storing all references is
// relatively high, so this is only used during patch generation.
class ImageIndex {
 public:
  ImageIndex(ConstBufferView image,
             std::vector<ReferenceTypeTraits>&& traits_map);
  ImageIndex(const ImageIndex&) = delete;
  ImageIndex(ImageIndex&& that);
  ~ImageIndex();

  // Inserts all references of type |type_tag| read from |ref_reader| to this
  // index. This should be called exactly once for each reference type. If
  // overlap between any two references of any type is encountered, returns
  // false and leaves the object in an invalid state. Otherwise, returns true.
  bool InsertReferences(TypeTag type_tag, ReferenceReader&& ref_reader);

  // Returns the number of reference type the index holds.
  size_t TypeCount() const { return types_.size(); }

  // Returns the number of target pool discovered.
  size_t PoolCount() const { return pools_.size(); }

  size_t LabelBound(PoolTag pool) const {
    DCHECK_LT(pool.value(), pools_.size());
    return pools_[pool.value()].label_bound;
  }

  // Returns traits describing references of type |type|.
  const ReferenceTypeTraits& GetTraits(TypeTag type) const {
    DCHECK_LT(type.value(), types_.size());
    return types_[type.value()].traits;
  }

  PoolTag GetPoolTag(TypeTag type) const { return GetTraits(type).pool_tag; }

  const std::vector<TypeTag>& GetTypeTags(PoolTag pool) const {
    DCHECK_LT(pool.value(), pools_.size());
    return pools_[pool.value()].types;
  }

  // Returns true if |raw_image_[location]| is either:
  // - A raw value.
  // - The first byte of a reference.
  bool IsToken(offset_t location) const;

  // Returns true if |raw_image_[location]| is part of a reference.
  bool IsReference(offset_t location) const {
    return GetType(location) != kNoTypeTag;
  }

  // Returns the type tag of the reference covering |location|, or kNoTypeTag if
  // |location| is not part of a reference.
  TypeTag GetType(offset_t location) const {
    DCHECK_EQ(type_tags_.size(), raw_image_.size());  // Sanity check.
    DCHECK_LT(location, size());
    return type_tags_[location];
  }

  // Returns the raw value at |location|.
  uint8_t GetRawValue(offset_t location) const {
    DCHECK_LT(location, size());
    return raw_image_[location];
  }

  // Returns the reference of type |type| covering |location|, which is expected
  // to exist.
  Reference FindReference(TypeTag type, offset_t location) const;

  // Returns a vector of references of type |type|, where references are sorted
  // by their location.
  const std::vector<Reference>& GetReferences(TypeTag type) const {
    DCHECK_LT(type.value(), types_.size());
    return types_[type.value()].references;
  }

  // Creates and returns a vector of all targets in |pool|.
  std::vector<offset_t> GetTargets(PoolTag pool) const;

  // Returns the size of the image.
  size_t size() const { return raw_image_.size(); }

  // Replaces every target represented as offset whose Label is in
  // |label_manager| by the index of this Label, and updates the Label bound
  // associated with |pool|.
  void LabelTargets(PoolTag pool, const BaseLabelManager& label_manager);

  // Replaces every associated target represented as offset whose Label is in
  // |label_manager| by the index of this Label, and updates the Label bound
  // associated with |pool|. A target is associated iff its Label index is also
  // used in |reference_label_manager|. All targets must have a Label in
  // |label_manager|, and must be represented as offset when calling this
  // function, implying it can only be called once for each |pool|, until
  // UnlabelTargets() (below) is called.
  void LabelAssociatedTargets(PoolTag pool,
                              const BaseLabelManager& label_manager,
                              const BaseLabelManager& reference_label_manager);

  // Replaces every target represented as a Label index by its original offset,
  // assuming that |label_manager| still holds the same Labels referered to by
  // target indices. Resets Label bound associated with |pool| to 0.
  void UnlabelTargets(PoolTag pool, const BaseLabelManager& label_manager);

 private:
  // Information stored for every reference type.
  struct TypeInfo {
    TypeInfo();
    TypeInfo(TypeInfo&&);
    ~TypeInfo();

    ReferenceTypeTraits traits;
    // List of Reference instances sorted by location (file offset).
    std::vector<Reference> references;
  };
  // Information stored for every reference pool.
  struct PoolInfo {
    PoolInfo();
    PoolInfo(PoolInfo&&);
    ~PoolInfo();

    std::vector<TypeTag> types;  // Enumerates type_tag for this pool.
    size_t label_bound = 0;      // Upper bound on Label indices for this pool.
  };

  const ConstBufferView raw_image_;

  // Used for random access lookup of reference type, for each byte in
  // |raw_image_|.
  std::vector<TypeTag> type_tags_;

  std::vector<TypeInfo> types_;
  std::vector<PoolInfo> pools_;
};

}  // namespace zucchini

#endif  // CHROME_INSTALLER_ZUCCHINI_IMAGE_INDEX_H_
