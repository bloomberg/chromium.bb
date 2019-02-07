// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_LOOKUP_TABLE_BUILDER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_LOOKUP_TABLE_BUILDER_WIN_H_

#include <dwrite.h>
#include <dwrite_2.h>
#include <wrl.h>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "content/common/content_export.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {

// Singleton class which encapsulates building the font unique name table lookup
// once, then serving the built table as a ReadOnlySharedMemoryRegion. Receives
// requests for accessing this table from DWriteFontProxyImpl after Mojo IPC
// calls from the renderer.
class CONTENT_EXPORT DWriteFontLookupTableBuilder {
 public:
  static DWriteFontLookupTableBuilder* GetInstance();
  void SetSlowDownIndexingForTesting(bool);
  // Needed to trigger rebuilding the lookup table, when testing using
  // slowed-down indexing. Otherwise, the test methods would use the already
  // cached lookup table.
  void ResetLookupTableForTesting();

  // Retrieve the prepared memory region if it is available.
  // EnsureFontUniqueNameTable() should be checked before. This method hits an
  // assertion otherwise.
  base::ReadOnlySharedMemoryRegion DuplicatedMemoryRegion();

  // Wait for the internal WaitableEvent to be signaled if needed and return
  // true if the font unique name lookup table was successfully constructed.
  bool EnsureFontUniqueNameTable();

 private:
  friend class base::NoDestructor<DWriteFontLookupTableBuilder>;

  void BuildFontUniqueNameTable();

  bool IsFontUniqueNameTableValid();

  // Checks if the unique font table has been built already, and if not, builds
  // it by enumerating fonts from the collection, extracting their file
  // locations, ttc indices and names.
  void InitializeDirectWrite();
  DWriteFontLookupTableBuilder();
  ~DWriteFontLookupTableBuilder();

  // This can only be replaced from the construction sequence. Once
  // font_table_built_ is signaled, it can be read from everywhere.
  base::MappedReadOnlyRegion font_table_memory_;

  bool direct_write_initialized_ = false;
  Microsoft::WRL::ComPtr<IDWriteFontCollection> collection_;
  Microsoft::WRL::ComPtr<IDWriteFactory2> factory2_;
  bool slow_down_indexing_for_testing_ = false;

  DISALLOW_COPY_AND_ASSIGN(DWriteFontLookupTableBuilder);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_LOOKUP_TABLE_BUILDER_WIN_H_
