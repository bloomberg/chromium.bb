// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_NACL_MOJO_SYSCALL_H_
#define MOJO_NACL_MOJO_SYSCALL_H_

// Injects a NaClDesc for Mojo support. This provides the implementation of the
// Mojo system API outside the NaCl sandbox.
void InjectMojo(struct NaClApp* nap);

// Injects a "disabled" NaClDesc for Mojo support. This is to make debugging
// more straightforward in the case where Mojo is not enabled for NaCl plugins.
void InjectDisabledMojo(struct NaClApp* nap);

#endif // MOJO_NACL_MOJO_SYSCALL_H_
