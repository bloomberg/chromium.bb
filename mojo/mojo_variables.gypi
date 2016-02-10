# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

#
# A set of GYP variables that are shared between various mojo .gyp files.
#
{
  'variables': {
    'chromium_code': 1,
    'mojo_shell_debug_url%': "",
    'conditions': [
      #
      # The following mojo_system-prefixed variables are used to express a
      # dependency on the mojo system APIs.
      #
      # In a component == "shared_library" build, everything can link against
      # mojo_system_impl because it is built as a shared library. However, in a
      # component != "shared_library" build, mojo_system_impl is linked into an
      # executable (e.g., mojo_shell), and must be injected into other shared
      # libraries (i.e., Mojo Apps) that need the mojo system API.
      #
      # For component targets, add <(mojo_system_for_component) to your
      # dependencies section.  For loadable module targets (e.g., a Mojo App),
      # add <(mojo_system_for_loadable_module) to your dependencies section.
      #
      # NOTE: component != "shared_library" implies that we are generating a
      # static library, and in that case, it is expected that the target
      # listing the component as a dependency will specify either mojo_system
      # or mojo_system_impl to link against. This enables multiple targets to
      # link against the same component library without having to agree on
      # which Mojo system library they are using.
      #
      ['component=="shared_library"', {
        'mojo_gles2_for_component': "<(DEPTH)/mojo/mojo_base.gyp:mojo_gles2_impl",
        'mojo_system_for_component': "<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl",
        'mojo_system_for_loadable_module': "<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl",
      }, {
        'mojo_gles2_for_component': "<(DEPTH)/mojo/mojo_base.gyp:mojo_none",
        'mojo_system_for_component': "<(DEPTH)/mojo/mojo_public.gyp:mojo_system_placeholder",
        'mojo_system_for_loadable_module': "<(DEPTH)/mojo/mojo_public.gyp:mojo_system",
      }],
    ],
    'mojo_public_system_unittest_sources': [
      '<(DEPTH)/mojo/public/c/system/tests/core_unittest.cc',
      '<(DEPTH)/mojo/public/c/system/tests/core_unittest_pure_c.c',
      '<(DEPTH)/mojo/public/c/system/tests/macros_unittest.cc',
      '<(DEPTH)/mojo/public/cpp/system/tests/core_unittest.cc',
      '<(DEPTH)/mojo/public/cpp/system/tests/macros_unittest.cc',
    ],
  },
}
