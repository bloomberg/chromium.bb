// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SideStep SideStep Library Overview
//
// The SideStep library includes two main components:
//  -# A preamble patching utility, enabling patching of most
//     functions.  See PreamblePatcher.
//  -# A mini disassembler, used by the preamble patching utility.  See
//   MiniDisassembler.
//
// To use the library, see integration.h; at a high level, you need to:
//  -# Link in a function sidestep::AssertImpl or explicitly define
//     the SIDESTEP_ASSERT macro.
//  -# Link in a function sidestep::LogImpl or explicitly define the
//     SIDESTEP_LOG macro.
//  -# Optional: Call sidestep::UnitTests() from from a unit test in your
//     code and check that it returns true (otherwise the unit test
//     found an error).
