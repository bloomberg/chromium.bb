// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_LABEL_MANAGER_H_
#define COURGETTE_LABEL_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "courgette/image_utils.h"

namespace courgette {

// A container to store and manage Label instances. A key consideration is peak
// memory usage reduction. To this end we preallocate Label instances in bulk,
// and carefully control transient memory usage when initializing Labels.
class LabelManager {
 public:
  // An adaptor to sequentially traverse multiple RVAs. This is useful for RVA
  // translation without extra storage. For example, we might have a stored list
  // of RVA locations, but we want to traverse the matching RVA targets.
  class RvaVisitor {
   public:
    virtual ~RvaVisitor();

    // Returns the number of remaining RVAs to visit.
    virtual size_t Remaining() const = 0;

    // Returns the current RVA.
    virtual RVA Get() const = 0;

    // Advances to the next RVA.
    virtual void Next() = 0;
  };

  LabelManager();
  virtual ~LabelManager();

  // Initializes |labels_| using RVAs from |rva_visitor|. Each distinct RVA from
  // |rva_visitor| yields a Label with |rva_| assigned as the RVA, and |count_|
  // assigned as the repeat.
  void Read(RvaVisitor* rva_visitor);

  // Removes |labels_| elements whose |count_| is less than |count_threshold|.
  void RemoveUnderusedLabels(int32_t count_threshold);

  // Efficiently searches for a Label that targets |rva|. Returns the pointer to
  // the stored Label instance if found, or null otherwise.
  Label* Find(RVA rva);

  // TODO(huangs): Move AssignRemainingIndexes() here.

 protected:
  // The main list of Label instances, sorted by the |rva_| member.
  std::vector<Label> labels_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LabelManager);
};

}  // namespace courgette

#endif  // COURGETTE_LABEL_MANAGER_H_
