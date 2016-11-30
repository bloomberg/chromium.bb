#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate chromium.gpu.json and chromium.gpu.fyi.json in
the src/testing/buildbot directory. Maintaining these files by hand is
too unwieldy.
"""

import copy
import json
import os
import string
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR)))

WATERFALL = {
  'builders': {
    'GPU Win Builder' : {},
    'GPU Win Builder (dbg)' : {},
    'GPU Mac Builder' : {},
    'GPU Mac Builder (dbg)' : {},
    'GPU Linux Builder' : {},
    'GPU Linux Builder (dbg)' : {},
   },

  'testers': {
    'Win7 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Mac 10.10 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

FYI_WATERFALL = {
  'builders': {
    'GPU Win Builder' : {},
    'GPU Win Builder (dbg)' : {},
    'GPU Win x64 Builder' : {},
    'GPU Win x64 Builder (dbg)' : {},
    'GPU Mac Builder' : {},
    'GPU Mac Builder (dbg)' : {},
    'GPU Linux Builder' : {},
    'GPU Linux Builder (dbg)' : {},
    'Linux ChromiumOS Builder' : {
      'additional_compile_targets' : [ "All" ]
    },
  },

  'testers': {
    'Win7 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win8 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2012ServerR2-SP0'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win8 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2012ServerR2-SP0'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:041a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Release (NVIDIA GeForce 730)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0f02',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Release (New Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0412',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Debug (New Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0412',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Release (AMD R7 240)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Release (AMD R5 230)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6779',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 x64 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release_x64',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 x64 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug_x64',
      'swarming': True,
      'os_type': 'win',
    },
    'Mac 10.10 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:679e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:679e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac Retina Release': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac Retina Debug': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.11 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off for testing purposes.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac GPU ASAN Release': {
      # This bot spawns jobs on multiple GPU types.
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.10'
        },
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Release (Intel Graphics Stack)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:041a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (AMD R5 230)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6779',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (NVIDIA GeForce 730)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0f02',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Release (New Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0412',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Debug (New Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0412',
          'os': 'Linux'
        },
      ],
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (AMD R7 240)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Android Release (Nexus 5)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Nexus 5X)': {
      'swarming_dimensions': [
        {
          'device_type': 'bullhead',
          'device_os': 'M',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      'swarming': True,
      'os_type': 'android',
    },
    'Android Release (Nexus 6)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Nexus 6P)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Nexus 9)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Pixel C)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },

    # The following "optional" testers don't actually exist on the
    # waterfall. They are present here merely to specify additional
    # tests which aren't on the main tryservers. Unfortunately we need
    # a completely different (redundant) bot specification to handle
    # this.
    'Optional Win7 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Optional Win7 Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Optional Mac 10.10 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Mac Retina Release': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

COMMON_GTESTS = {
  'angle_deqp_egl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Win7 Release NVIDIA 32- and 64-bit bots
        # (and trybots) for the time being, at least until more capacity is
        # added.
        # TODO(jmadill): Run on the Linux Release NVIDIA bots.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          }
        ],
      },
    ],
  },

  'angle_deqp_gles2_d3d11_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Win7 NVIDIA/AMD R7 240 32- and 64-bit bots (and
        # trybots) for the time being, at least until more capacity is
        # added.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '1002:6613',
            'os': 'Windows-2008ServerR2-SP1'
          },
        ],
      },
    ],
    'desktop_swarming': {
      'shards': 4,
    },
    'test': 'angle_deqp_gles2_tests',
    'args': ['--deqp-egl-display-type=angle-d3d11']
  },

  'angle_deqp_gles2_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Linux Release NVIDIA 32- and 64-bit bots (and
        # trybots) for the time being, at least until more capacity is added.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          },
        ],
      },
    ],
    'desktop_swarming': {
      'shards': 4,
    },
    'test': 'angle_deqp_gles2_tests',
    'args': ['--deqp-egl-display-type=angle-gl'],
  },

  'angle_deqp_gles2_gles_tests': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run on Nexus 5X swarmed bots.
        'build_configs': ['android-chromium'],
        'swarming_dimension_sets': [
          {
            'device_type': 'bullhead',
            'device_os': 'M',
            'os': 'Android'
          }
        ],
      },
    ],
    'test': 'angle_deqp_gles2_tests',
    # Only pass the display type to desktop. The Android runner doesn't support
    # passing args to the executable but only one display type is supported on
    # Android anyways.
    'desktop_args': ['--deqp-egl-display-type=angle-gles'],
    'android_args': ['--enable-xml-result-parsing']
  },

  'angle_deqp_gles3_d3d11_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # TODO(jmadill): Run this on the optional tryservers.
        'run_on_optional': False,
        # Run only on the Win7 Release NVIDIA 32-bit bots (and trybots) for the
        # time being, at least until more capacity is added.
        # TODO(jmadill): Run on the Win AMD R7 240 bots once they are swarmed.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          }
        ],
      }
    ],
    'swarming': {
      'shards': 12,
    },
    'test': 'angle_deqp_gles3_tests',
    'args': ['--deqp-egl-display-type=angle-d3d11']
  },

  'angle_deqp_gles3_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # TODO(jmadill): Run this on the optional tryservers.
        'run_on_optional': False,
        # Run only on the Linux Release NVIDIA 32-bit bots (and trybots) for
        # the time being, at least until more capacity is added.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          }
        ],
      }
    ],
    'swarming': {
      'shards': 12,
    },
    'test': 'angle_deqp_gles3_tests',
    'args': ['--deqp-egl-display-type=angle-gl']
  },

  # Until we have more capacity, run angle_end2end_tests only on the
  # FYI waterfall, the ANGLE trybots (which mirror the FYI waterfall),
  # and the optional trybots (mainly used during ANGLE rolls).
  'angle_end2end_tests': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'fyi_only': True,
        'run_on_optional': True,
      },
    ],
    'disabled_tester_configs': [
      {
        'names': [
          # TODO(ynovikov) Investigate why the test breaks on older devices.
          'Android Release (Nexus 5)',
          'Android Release (Nexus 6)',
          'Android Release (Nexus 9)',
        ],
      },
    ],
    'desktop_args': ['--use-gpu-in-tests']
  },
  'angle_unittests': {
    'tester_configs': [
      {
        'allow_on_android': True,
      }
    ],
    'desktop_args': ['--use-gpu-in-tests']
  },
  # Until the media-only tests are extracted from content_unittests,
  # and audio_unittests and content_unittests can be run on the commit
  # queue with --require-audio-hardware-for-testing, run them only on
  # the FYI waterfall.
  #
  # Note that the transition to the Chromium recipe has forced the
  # removal of the --require-audio-hardware-for-testing flag for the
  # time being. See crbug.com/574942.
  'audio_unittests': {
    'tester_configs': [
      {
        'fyi_only': True,
      }
    ],
    'args': ['--use-gpu-in-tests']
  },
  # TODO(kbr): content_unittests is killing the Linux GPU swarming
  # bots. crbug.com/582094 . It's not useful now anyway until audio
  # hardware is deployed on the swarming bots, so stop running it
  # everywhere.
  # 'content_unittests': {},
  'gl_tests': {
    'tester_configs': [
      {
        'allow_on_android': True,
      }
    ],
    'disabled_tester_configs': [
      {
        'names': [
          # TODO(kbr): investigate inability to recognize this
          # configuration in the various tests. crbug.com/624621
          'Android Release (Pixel C)',
        ],
      },
    ],
    'desktop_args': ['--use-gpu-in-tests']
  },
  'gl_unittests': {
    'tester_configs': [
      {
        'allow_on_android': True,
      }
    ],
    'desktop_args': ['--use-gpu-in-tests']
  },
  # The gles2_conform_tests are closed-source and deliberately only run
  # on the FYI waterfall and the optional tryservers.
  'gles2_conform_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
      }
    ],
    'args': ['--use-gpu-in-tests']
  },
  'gles2_conform_d3d9_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        # Run this on the optional tryservers.
        'run_on_optional': True,
      }
    ],
    'args': [
      '--use-gpu-in-tests',
      '--use-angle=d3d9',
    ],
    'test': 'gles2_conform_test',
  },
  'gles2_conform_gl_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        # Run this on the optional tryservers.
        'run_on_optional': True,
      }
    ],
    'args': [
      '--use-gpu-in-tests',
      '--use-angle=gl',
      '--disable-gpu-sandbox',
    ],
    'test': 'gles2_conform_test',
  },
  'tab_capture_end2end_tests': {
    'tester_configs': [
      {
        'build_configs': ['Release', 'Release_x64'],
      }
    ],
    'override_compile_targets': [
      'tab_capture_end2end_tests_run',
    ],
  },
}

# This requires a hack because the isolate's name is different than
# the executable's name. On the few non-swarmed testers, this causes
# the executable to not be found. It would be better if the Chromium
# recipe supported running isolates locally. crbug.com/581953

NON_SWARMED_GTESTS = {
  'tab_capture_end2end_tests': {
     'swarming': {
       'can_use_on_swarming_builders': False
     },
     'test': 'browser_tests',
     'args': [
       '--enable-gpu',
       '--test-launcher-jobs=1',
       '--gtest_filter=CastStreamingApiTestWithPixelOutput.EndToEnd*:' + \
           'TabCaptureApiPixelTest.EndToEnd*'
     ],
     'swarming': {
       'can_use_on_swarming_builders': False,
     },
  }
}

TELEMETRY_TESTS = {
  'gpu_process_launch_tests': {
      'target_name': 'gpu_process',
      'tester_configs': [
        {
          'allow_on_android': True,
        }
      ],
  },
  'gpu_rasterization': {
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
  'hardware_accelerated_feature': {
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
  'maps_pixel_test': {
    'target_name': 'maps',
    'args': [
      '--os-type',
      '${os_type}',
      '--build-revision',
      '${got_revision}',
      '--test-machine-name',
      '${buildername}',
    ],
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
  'screenshot_sync': {
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
  'trace_test': {
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
}

# These tests use Telemetry's new, simpler, browser_test_runner.
# Eventually all of the Telemetry based tests above will be ported to
# this harness, and the old harness will be deleted.
TELEMETRY_GPU_INTEGRATION_TESTS = {
  'context_lost': {
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ]
  },
  'pixel_test': {
    'target_name': 'pixel',
    'args': [
      '--refimg-cloud-storage-bucket',
      'chromium-gpu-archive/reference-images',
      '--os-type',
      '${os_type}',
      '--build-revision',
      '${got_revision}',
      '--test-machine-name',
      '${buildername}',
    ],
    'non_precommit_args': [
      '--upload-refimg-to-cloud-storage',
    ],
    'precommit_args': [
      '--download-refimg-from-cloud-storage',
    ],
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
  'webgl_conformance': {
    'tester_configs': [
      {
        'allow_on_android': True,
      },
    ],
  },
  'webgl_conformance_d3d9_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        'run_on_optional': True,
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=d3d9',
    ],
  },
  'webgl_conformance_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        'run_on_optional': True,
      }
    ],
    'disabled_tester_configs': [
      {
        'swarming_dimension_sets': [
          # crbug.com/555545 and crbug.com/649824:
          # Disable webgl_conformance_gl_tests on some Win/AMD cards.
          # Always fails on older cards, flaky on newer cards.
          # Note that these must match the GPUs exactly; wildcard
          # matches (i.e., only device ID) aren't supported!
          {
            'gpu': '1002:6779',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '1002:6613',
            'os': 'Windows-2008ServerR2-SP1'
          },
          # BUG 590951: Disable webgl_conformance_gl_tests on Win/Intel
          {
            'gpu': '8086:041a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '8086:0412',
            'os': 'Windows-2008ServerR2-SP1'
          },
        ],
      },
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=gl',
    ],
  },
  'webgl_conformance_angle_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['linux'],
        'run_on_optional': True,
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-gl=angle',
    ],
  },
  'webgl2_conformance_tests': {
    'tester_configs': [
      {
         # The WebGL 2.0 conformance tests take over an hour to run on
         # the Debug bots, which is too long.
        'build_configs': ['Release', 'Release_x64'],
        'fyi_only': True,
        'run_on_optional': True,
      },
    ],
    'disabled_tester_configs': [
      {
        'names': [
          # http://crbug.com/599451: this test is currently too slow
          # to run on x64 in Debug mode. Need to shard the tests.
          'Win7 x64 Debug (NVIDIA)',
        ],
      },
    ],
    'target_name': 'webgl_conformance',
    'args': [
      '--webgl-conformance-version=2.0.0',
      # The current working directory when run via isolate is
      # out/Debug or out/Release. Reference this file relatively to
      # it.
      '--read-abbreviated-json-results-from=' + \
      '../../content/test/data/gpu/webgl2_conformance_tests_output.json',
    ],
    'swarming': {
      # These tests currently take about an hour and fifteen minutes
      # to run. Split them into roughly 5-minute shards.
      'shards': 15,
    },
  },
  'webgl2_conformance_angle_tests': {
    'tester_configs': [
      {
         # The WebGL 2.0 conformance tests take over an hour to run on
         # the Debug bots, which is too long.
        'build_configs': ['Release'],
        'fyi_only': True,
        'run_on_optional': False,
        # Only run on the NVIDIA Release and New Intel Release Linux bots
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          },
          {
            'gpu': '8086:0412',
            'os': 'Linux'
          },
        ],
      },
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-gl=angle',
    ],
    'args': [
      '--webgl-conformance-version=2.0.0',
      # The current working directory when run via isolate is
      # out/Debug or out/Release. Reference this file relatively to
      # it.
      '--read-abbreviated-json-results-from=' + \
      '../../content/test/data/gpu/webgl2_conformance_tests_output.json',
    ],
    'swarming': {
      # These tests currently take about an hour and fifteen minutes
      # to run. Split them into roughly 5-minute shards.
      'shards': 15,
    },
  },
}

def substitute_args(tester_config, args):
  """Substitutes the ${os_type} variable in |args| from the
     tester_config's "os_type" property.
  """
  substitutions = {
    'os_type': tester_config['os_type']
  }
  return [string.Template(arg).safe_substitute(substitutions) for arg in args]

def matches_swarming_dimensions(tester_config, dimension_sets):
  for dimensions in dimension_sets:
    for cur_dims in tester_config['swarming_dimensions']:
      if set(dimensions.items()).issubset(cur_dims.items()):
        return True
  return False

def is_android(tester_config):
  return tester_config['os_type'] == 'android'

def tester_config_matches_tester(tester_name, tester_config, tc, is_fyi,
                                 check_waterfall):
  if check_waterfall:
    if tc.get('fyi_only', False) and not is_fyi:
      return False
    # Handle the optional tryservers with the 'run_on_optional' flag.
    # Only a subset of the tests run on these tryservers.
    if tester_name.startswith('Optional') and not tc.get(
        'run_on_optional', False):
      return False

  if 'names' in tc:
    # Give priority to matching the tester_name.
    if tester_name in tc['names']:
      return True
    if not tester_name in tc['names']:
      return False
  if 'os_types' in tc:
    if not tester_config['os_type'] in tc['os_types']:
      return False
  if 'build_configs' in tc:
    if not tester_config['build_config'] in tc['build_configs']:
      return False
  if 'swarming_dimension_sets' in tc:
    if not matches_swarming_dimensions(tester_config,
                                       tc['swarming_dimension_sets']):
      return False
  if is_android(tester_config):
    if not tc.get('allow_on_android', False):
      return False
  return True

def should_run_on_tester(tester_name, tester_config, test_config, is_fyi):
  # Check if this config is disabled on this tester
  if 'disabled_tester_configs' in test_config:
    for dtc in test_config['disabled_tester_configs']:
      if tester_config_matches_tester(tester_name, tester_config, dtc, is_fyi,
                                      False):
        return False
  if 'tester_configs' in test_config:
    for tc in test_config['tester_configs']:
      if tester_config_matches_tester(tester_name, tester_config, tc, is_fyi,
                                      True):
        return True
    return False
  else:
    # If tester_configs is unspecified, run nearly all tests by default,
    # but let tester_config_matches_tester filter out any undesired
    # tests, such as ones that should only run on the Optional bots.
    return tester_config_matches_tester(tester_name, tester_config, {},
                                        is_fyi, True)

def generate_gtest(tester_name, tester_config, test, test_config, is_fyi):
  if not should_run_on_tester(tester_name, tester_config, test_config, is_fyi):
    return None
  result = copy.deepcopy(test_config)
  if 'tester_configs' in result:
    # Don't print the tester_configs in the JSON.
    result.pop('tester_configs')
  if 'disabled_tester_configs' in result:
    # Don't print the disabled_tester_configs in the JSON.
    result.pop('disabled_tester_configs')
  if 'test' in result:
    result['name'] = test
  else:
    result['test'] = test
  if (not tester_config['swarming']) and test in NON_SWARMED_GTESTS:
    # Need to override this result.
    result = copy.deepcopy(NON_SWARMED_GTESTS[test])
    result['name'] = test
  else:
    # Put the swarming dimensions in anyway. If the tester is later
    # swarmed, they will come in handy.
    if not 'swarming' in result:
      result['swarming'] = {}
    result['swarming'].update({
      'can_use_on_swarming_builders': True,
      'dimension_sets': tester_config['swarming_dimensions']
    })
    if is_android(tester_config):
      # Override the isolate target to get rid of any "_apk" suffix
      # that would be added by the recipes.
      if 'test' in result:
        result['override_isolate_target'] = result['test']
      else:
        result['override_isolate_target'] = result['name']
      # Integrate with the unified logcat system.
      result['swarming'].update({
        'cipd_packages': [
          {
            'cipd_package': 'infra/tools/luci/logdog/butler/${platform}',
            'location': 'bin',
            'revision': 'git_revision:3ff24775a900b675866fbcacf2a8f98a18b2a16a'
          }
        ],
        'output_links': [
          {
            'link': [
              'https://luci-logdog.appspot.com/v/?s',
              '=android%2Fswarming%2Flogcats%2F',
              '${TASK_ID}%2F%2B%2Funified_logcats'
            ],
            'name': 'shard #${SHARD_INDEX} logcats'
          }
        ]
      })
  if 'desktop_args' in result:
    if not is_android(tester_config):
      if not 'args' in result:
        result['args'] = []
      result['args'] += result['desktop_args']
    # Don't put the desktop args in the JSON.
    result.pop('desktop_args')
  if 'android_args' in result:
    if is_android(tester_config):
      if not 'args' in result:
        result['args'] = []
      result['args'] += result['android_args']
    # Don't put the android args in the JSON.
    result.pop('android_args')
  if 'desktop_swarming' in result:
    if not is_android(tester_config):
      result['swarming'].update(result['desktop_swarming'])
    # Don't put the desktop_swarming in the JSON.
    result.pop('desktop_swarming')

  # This flag only has an effect on the Linux bots that run tests
  # locally (as opposed to via Swarming), which are only those couple
  # on the chromium.gpu.fyi waterfall. Still, there is no harm in
  # specifying it everywhere.
  result['use_xvfb'] = False
  return result

def generate_telemetry_test(tester_name, tester_config,
                            test, test_config, is_fyi,
                            use_gpu_integration_test_harness):
  if not should_run_on_tester(tester_name, tester_config, test_config, is_fyi):
    return None
  test_args = ['-v']
  # --expose-gc allows the WebGL conformance tests to more reliably
  # reproduce GC-related bugs in the V8 bindings.
  extra_browser_args_string = (
      '--enable-logging=stderr --js-flags=--expose-gc')
  if 'extra_browser_args' in test_config:
    extra_browser_args_string += ' ' + ' '.join(
        test_config['extra_browser_args'])
  test_args.append('--extra-browser-args=' + extra_browser_args_string)
  if 'args' in test_config:
    test_args.extend(substitute_args(tester_config, test_config['args']))
  if 'desktop_args' in test_config and not is_android(tester_config):
    test_args.extend(substitute_args(tester_config,
                                     test_config['desktop_args']))
  if 'android_args' in test_config and is_android(tester_config):
    test_args.extend(substitute_args(tester_config,
                                     test_config['android_args']))
  # The step name must end in 'test' or 'tests' in order for the
  # results to automatically show up on the flakiness dashboard.
  # (At least, this was true some time ago.) Continue to use this
  # naming convention for the time being to minimize changes.
  step_name = test
  if not (step_name.endswith('test') or step_name.endswith('tests')):
    step_name = '%s_tests' % step_name
  # Prepend Telemetry GPU-specific flags.
  benchmark_name = test_config.get('target_name') or test
  prefix_args = [
    benchmark_name,
    '--show-stdout',
    '--browser=%s' % tester_config['build_config'].lower()
  ]
  swarming = {
    # Always say this is true regardless of whether the tester
    # supports swarming. It doesn't hurt.
    'can_use_on_swarming_builders': True,
    'dimension_sets': tester_config['swarming_dimensions']
  }
  if 'swarming' in test_config:
    swarming.update(test_config['swarming'])
  result = {
    'args': prefix_args + test_args,
    'isolate_name': (
      'telemetry_gpu_integration_test' if use_gpu_integration_test_harness
      else 'telemetry_gpu_test'),
    'name': step_name,
    'override_compile_targets': [
      ('telemetry_gpu_integration_test_run' if use_gpu_integration_test_harness
       else 'telemetry_gpu_test_run')
    ],
    'swarming': swarming,
  }
  if 'non_precommit_args' in test_config:
    result['non_precommit_args'] = test_config['non_precommit_args']
  if 'precommit_args' in test_config:
    result['precommit_args'] = test_config['precommit_args']
  return result

def generate_gtests(tester_name, tester_config, test_dictionary, is_fyi):
  # The relative ordering of some of the tests is important to
  # minimize differences compared to the handwritten JSON files, since
  # Python's sorts are stable and there are some tests with the same
  # key (see gles2_conform_d3d9_test and similar variants). Avoid
  # losing the order by avoiding coalescing the dictionaries into one.
  gtests = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_gtest(tester_name, tester_config,
                          test_name, test_config, is_fyi)
    if test:
      # generate_gtest may veto the test generation on this platform.
      gtests.append(test)
  return gtests

def generate_telemetry_tests(tester_name, tester_config,
                             test_dictionary, is_fyi,
                             use_gpu_integration_test_harness):
  isolated_scripts = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_telemetry_test(
      tester_name, tester_config, test_name, test_config, is_fyi,
      use_gpu_integration_test_harness)
    if test:
      isolated_scripts.append(test)
  return isolated_scripts

def generate_all_tests(waterfall, is_fyi):
  tests = {}
  for builder, config in waterfall['builders'].iteritems():
    tests[builder] = config
  for name, config in waterfall['testers'].iteritems():
    gtests = generate_gtests(name, config, COMMON_GTESTS, is_fyi)
    isolated_scripts = \
      generate_telemetry_tests(name, config, TELEMETRY_TESTS, is_fyi, False) + \
      generate_telemetry_tests(name, config, TELEMETRY_GPU_INTEGRATION_TESTS,
                               is_fyi, True)
    tests[name] = {
      'gtest_tests': sorted(gtests, key=lambda x: x['test']),
      'isolated_scripts': sorted(isolated_scripts, key=lambda x: x['name'])
    }
  tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
  tests['AAAAA2 See generate_buildbot_json.py to make changes'] = {}
  filename = 'chromium.gpu.fyi.json' if is_fyi else 'chromium.gpu.json'
  with open(os.path.join(SRC_DIR, 'testing', 'buildbot', filename), 'w') as fp:
    json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
    fp.write('\n')

def main():
  generate_all_tests(FYI_WATERFALL, True)
  generate_all_tests(WATERFALL, False)
  return 0

if __name__ == "__main__":
  sys.exit(main())
