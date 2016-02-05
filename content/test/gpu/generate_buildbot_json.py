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
  'builders': [
    'GPU Win Builder',
    'GPU Win Builder (dbg)',
    'GPU Mac Builder',
    'GPU Mac Builder (dbg)',
    'GPU Linux Builder',
    'GPU Linux Builder (dbg)',
   ],

  'testers': {
    'Win7 Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (ATI)': {
      'swarming_dimensions': {
        'gpu': '1002:6779',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:041a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Mac 10.10 Release (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:0a2e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:0a2e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac Retina Release': {
      'swarming_dimensions': {
        'gpu': '10de:0fe9',
        'hidpi': '1',
        'os': 'Mac'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac Retina Debug': {
      'swarming_dimensions': {
        'gpu': '10de:0fe9',
        'hidpi': '1',
        'os': 'Mac'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': {
        'gpu': '1002:6821',
        'hidpi': '1',
        'os': 'Mac-10.10'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Debug (AMD)': {
      'swarming_dimensions': {
        'gpu': '1002:6821',
        'hidpi': '1',
        'os': 'Mac-10.10'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Linux'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Debug (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Linux'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

FYI_WATERFALL = {
  'builders': [
    'GPU Win Builder',
    'GPU Win Builder (dbg)',
    'GPU Win x64 Builder',
    'GPU Mac Builder',
    'GPU Mac Builder (dbg)',
    'GPU Linux Builder',
    'GPU Linux Builder (dbg)',
   ],

  'testers': {
    'Win7 Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win8 Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2012ServerR2-SP0'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win8 Debug (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2012ServerR2-SP0'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (ATI)': {
      'swarming_dimensions': {
        'gpu': '1002:6779',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release dEQP (NVIDIA)': {
      'deqp': True,
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 x64 Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release_x64',
      'swarming': True,
      'os_type': 'win',
    },
    'Mac 10.10 Release (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:0a2e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:0a2e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Release (ATI)': {
      'swarming_dimensions': {
        'gpu': '1002:679e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (ATI)': {
      'swarming_dimensions': {
        'gpu': '1002:679e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac Retina Release': {
      'swarming_dimensions': {
        'gpu': '10de:0fe9',
        'hidpi': '1',
        'os': 'Mac'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac Retina Debug': {
      'swarming_dimensions': {
        'gpu': '10de:0fe9',
        'hidpi': '1',
        'os': 'Mac'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': {
        'gpu': '1002:6821',
        'hidpi': '1',
        'os': 'Mac-10.10'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Debug (AMD)': {
      'swarming_dimensions': {
        'gpu': '1002:6821',
        'hidpi': '1',
        'os': 'Mac-10.10'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Linux'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Release (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:041a',
        'os': 'Linux'
      },
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (Intel Graphics Stack)': {
      'swarming_dimensions': {
        'gpu': '8086:041a',
        'os': 'Linux'
      },
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (ATI)': {
      'swarming_dimensions': {
        'gpu': '1002:6779',
        'os': 'Linux'
      },
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Debug (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Linux'
      },
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Release dEQP (NVIDIA)': {
      'deqp': True,
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Linux'
      },
      'build_config': 'Release',
      # TODO(kbr): switch this to use Swarming, and put the physical
      # machine into the Swarming pool, once we're convinced it's
      # working well.
      'swarming': False,
      'os_type': 'linux',
    },

    # The following "optional" testers don't actually exist on the
    # waterfall. They are present here merely to specify additional
    # tests which aren't on the main tryservers. Unfortunately we need
    # a completely different (redundant) bot specification to handle
    # this.
    'Optional Win7 Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Optional Win7 Release (ATI)': {
      'swarming_dimensions': {
        'gpu': '1002:6779',
        'os': 'Windows-2008ServerR2-SP1'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Optional Mac 10.10 Release (Intel)': {
      'swarming_dimensions': {
        'gpu': '8086:0a2e',
        'os': 'Mac-10.10'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Mac Retina Release': {
      'swarming_dimensions': {
        'gpu': '10de:0fe9',
        'hidpi': '1',
        'os': 'Mac'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Linux Release (NVIDIA)': {
      'swarming_dimensions': {
        'gpu': '10de:104a',
        'os': 'Linux'
      },
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

COMMON_GTESTS = {
  'angle_deqp_gles2_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Win7 and Linux Release NVIDIA 32- and 64-bit bots
        # (and trybots) for the time being, at least until more capacity is
        # added.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          }
        ],
      },
    ],
    'swarming_shards': 4
  },
  # Until we have more capacity, run angle_end2end_tests only on the
  # FYI waterfall, the ANGLE trybots (which mirror the FYI waterfall),
  # and the optional trybots (mainly used during ANGLE rolls).
  'angle_end2end_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'run_on_optional': True,
      },
    ],
    'args': ['--use-gpu-in-tests']
  },
  'angle_unittests': {'args': ['--use-gpu-in-tests']},
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
  'content_gl_tests': {'args': ['--use-gpu-in-tests']},
  # TODO(kbr): content_unittests is killing the Linux GPU swarming
  # bots. crbug.com/582094 . It's not useful now anyway until audio
  # hardware is deployed on the swarming bots, so stop running it
  # everywhere.
  # 'content_unittests': {},
  'gl_tests': {'args': ['--use-gpu-in-tests']},
  'gl_unittests': {'args': ['--use-gpu-in-tests']},
  # The gles2_conform_tests are closed-source and deliberately only run
  # on the FYI waterfall.
  'gles2_conform_test': {
    'tester_configs': [
      {
        'fyi_only': True,
      }
    ],
    'args': ['--use-gpu-in-tests']
  },
  'gles2_conform_d3d9_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win']
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
        'os_types': ['win']
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
     'test': 'browser_tests',
     'args': [
       '--enable-gpu',
       '--test-launcher-jobs=1',
       '--gtest_filter=CastStreamingApiTestWithPixelOutput.EndToEnd*:' + \
           'TabCaptureApiPixelTest.EndToEnd*'
     ]
  }
}

DEQP_GTESTS = {
  'angle_deqp_gles3_tests': {
    'tester_configs': [
      {
        'os_types': ['win', 'linux']
      }
    ],
    'swarming_shards': 12
  },
}

TELEMETRY_TESTS = {
  'context_lost': {},
  'gpu_process_launch_tests': {'target_name': 'gpu_process'},
  'gpu_rasterization': {},
  'hardware_accelerated_feature': {},
  'maps_pixel_test': {'target_name': 'maps'},
  'memory_test': {},
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
    ]
  },
  'screenshot_sync': {},
  'trace_test': {},
  'webgl_conformance': {},
  'webgl_conformance_d3d9_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'run_on_optional': True,
        'os_types': ['win']
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=d3d9',
    ],
  },
  # TODO(jmadill): run this on the optional tryservers once AMD/Win is fixed.
  'webgl_conformance_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win']
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=gl',
    ],
  },
  'webgl2_conformance_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'run_on_optional': True,
      },
    ],
    'target_name': 'webgl_conformance',
    'args': [
      '--webgl-conformance-version=2.0.0',
      '--webgl2-only=true',
    ],
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
    if set(dimensions.items()).issubset(
        tester_config['swarming_dimensions'].items()):
      return True
  return False

def should_run_on_tester_impl(tester_name, tester_config, tc, is_fyi):
  if tc.get('fyi_only', False) and not is_fyi:
    return False
  # Handle the optional tryservers with the 'run_on_optional' flag.
  # Only a subset of the tests run on these tryservers.
  if tester_name.startswith('Optional') and not tc.get(
      'run_on_optional', False):
    return False

  if 'names' in tc:
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
  return True

def should_run_on_tester(tester_name, tester_config, test_config, is_fyi):
  if not 'tester_configs' in test_config:
    # Filter out tests from the "optional" bots.
    if tester_name.startswith('Optional'):
      return False
    # Otherwise, if unspecified, run on all testers.
    return True
  for tc in test_config['tester_configs']:
    if should_run_on_tester_impl(tester_name, tester_config, tc, is_fyi):
      return True
  return False

def generate_gtest(tester_name, tester_config, test, test_config, is_fyi):
  if not should_run_on_tester(tester_name, tester_config, test_config, is_fyi):
    return None
  result = copy.deepcopy(test_config)
  if 'tester_configs' in result:
    # Don't print the tester_configs in the JSON.
    result.pop('tester_configs')
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
    result['swarming'] = {
      'can_use_on_swarming_builders': True,
      'dimension_sets': [
        tester_config['swarming_dimensions']
      ],
    }
    if result.get('swarming_shards'):
      result['swarming']['shards'] = result['swarming_shards']
      result.pop('swarming_shards')
  return result

def generate_telemetry_test(tester_name, tester_config,
                            test, test_config, is_fyi):
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
  result = {
    'args': prefix_args + test_args,
    'isolate_name': 'telemetry_gpu_test',
    'name': step_name,
    'override_compile_targets': [
      'telemetry_gpu_test_run'
    ],
    'swarming': {
      # Always say this is true regardless of whether the tester
      # supports swarming. It doesn't hurt.
      'can_use_on_swarming_builders': True,
      'dimension_sets': [
        tester_config['swarming_dimensions']
      ]
    }
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
                             test_dictionary, is_fyi):
  isolated_scripts = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_telemetry_test(
        tester_name, tester_config, test_name, test_config, is_fyi)
    if test:
      isolated_scripts.append(test)
  return isolated_scripts

def generate_all_tests(waterfall, is_fyi):
  tests = {}
  for builder in waterfall['builders']:
    tests[builder] = {}
  for name, config in waterfall['testers'].iteritems():
    gtests = []
    if config.get('deqp'):
      gtests.extend(generate_gtests(name, config, DEQP_GTESTS, is_fyi))
    else:
      gtests.extend(generate_gtests(name, config, COMMON_GTESTS, is_fyi))
    isolated_scripts = []
    if not config.get('deqp'):
      isolated_scripts.extend(generate_telemetry_tests(
          name, config, TELEMETRY_TESTS, is_fyi))
    cur_tests = {}
    if gtests:
      cur_tests['gtest_tests'] = sorted(gtests, key=lambda x: x['test'])
    if isolated_scripts:
      cur_tests['isolated_scripts'] = sorted(
          isolated_scripts, key=lambda x: x['name'])
    tests[name] = cur_tests
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
