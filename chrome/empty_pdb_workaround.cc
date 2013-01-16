// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used to generate an empty .pdb -- with a 4K pagesize -- that we
// then use during the final link for chrome.dll's pdb. Otherwise, the linker
// will generate a pdb with a page size of 1K, which imposes a limit of 1G on
// the .pdb which we exceed. By generating a .pdb with the compiler (rather
// than the linker), this limit is avoided.
