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
import string
import sys

BUILDERS = [
  'GPU NextGen Linux Builder',
  'GPU NextGen Linux Builder (dbg)',
  'GPU NextGen Mac Builder',
  'GPU NextGen Mac Builder (dbg)',
  'GPU NextGen Win Builder',
  'GPU NextGen Win Builder (dbg)',
]

TESTERS = {
  'Linux NextGen Debug (NVIDIA)': {
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Linux'
    },
    'build_config': 'Debug',
    'swarming': True,
    'os_type': 'linux',
  },
  'Linux NextGen Release (NVIDIA)': {
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Linux'
    },
    'build_config': 'Release',
    'swarming': True,
    'os_type': 'linux',
  },
  'Mac 10.10 Debug (ATI)': {
    'swarming_dimensions': {
      'gpu': '1002:679e',
      'os': 'Mac-10.10'
    },
    'build_config': 'Debug',
    'swarming': False,
    'os_type': 'mac',
  },
  'Mac 10.10 Release (ATI)': {
    'swarming_dimensions': {
      'gpu': '1002:679e',
      'os': 'Mac-10.10'
    },
    'build_config': 'Release',
    'swarming': False,
    'os_type': 'mac',
  },
  'Mac NextGen 10.10 Retina Debug (AMD)': {
    'swarming_dimensions': {
      'gpu': '1002:6821',
      'hidpi': '1',
      'os': 'Mac-10.10'
    },
    'build_config': 'Debug',
    'swarming': True,
    'os_type': 'mac',
  },
  'Mac NextGen 10.10 Retina Release (AMD)': {
    'swarming_dimensions': {
      'gpu': '1002:6821',
      'hidpi': '1',
      'os': 'Mac-10.10'
    },
    'build_config': 'Release',
    'swarming': True,
    'os_type': 'mac',
  },
  'Win7 NextGen Debug (NVIDIA)': {
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Windows-2008ServerR2-SP1'
    },
    'build_config': 'Debug',
    'swarming': True,
    'os_type': 'win',
  },
  'Win7 NextGen Release (AMD)': {
    'swarming_dimensions': {
      'gpu': '1002:6779',
      'os': 'Windows-2008ServerR2-SP1'
    },
    'build_config': 'Release',
    'swarming': True,
    'os_type': 'win',
  },
  'Win7 NextGen Release (Intel)': {
    'swarming_dimensions': {
      'gpu': '8086:041a',
      'os': 'Windows-2008ServerR2-SP1'
    },
    'build_config': 'Release',
    'swarming': False,
    'os_type': 'win',
  },
  'Win7 NextGen Release (NVIDIA)': {
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Windows-2008ServerR2-SP1'
    },
    'build_config': 'Release',
    'swarming': True,
    'os_type': 'win',
  },
  'Win7 NextGen dEQP (NVIDIA)': {
    'deqp': True,
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Windows-2008ServerR2-SP1'
    },
    'build_config': 'Release',
    'swarming': True,
    'os_type': 'win',
  },
  'Win8 NextGen Debug (NVIDIA)': {
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Windows-2012ServerR2-SP0'
    },
    'build_config': 'Debug',
    'swarming': True,
    'os_type': 'win',
  },
  'Win8 NextGen Release (NVIDIA)': {
    'swarming_dimensions': {
      'gpu': '10de:104a',
      'os': 'Windows-2012ServerR2-SP0'
    },
    'build_config': 'Release',
    'swarming': True,
    'os_type': 'win',
  },
}

COMMON_GTESTS = {
  'angle_end2end_tests': {'args': ['--use-gpu-in-tests']},
  'angle_unittests': {'args': ['--use-gpu-in-tests']},
  'content_gl_tests': {'args': ['--use-gpu-in-tests']},
  'gl_tests': {'args': ['--use-gpu-in-tests']},
  'gl_unittests': {'args': ['--use-gpu-in-tests']},
}

RELEASE_ONLY_GTESTS = {
  'tab_capture_end2end_tests': {
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

# Until the media-only tests are extracted from content_unittests and
# these both can be run on the commit queue with
# --require-audio-hardware-for-testing, run them only on the FYI
# waterfall.
#
# Note that the transition to the Chromium recipe has forced the
# removal of the --require-audio-hardware-for-testing flag for the
# time being. See crbug.com/574942.
FYI_ONLY_GTESTS = {
  'audio_unittests': {'args': ['--use-gpu-in-tests']},
  # TODO(kbr): content_unittests is killing the Linux GPU swarming
  # bots. crbug.com/582094 . It's not useful now anyway until audio
  # hardware is deployed on the swarming bots, so stop running it
  # everywhere.
  # 'content_unittests': {},
  # The gles2_conform_tests are closed-source and deliberately only run
  # on the FYI waterfall.
  'gles2_conform_test': {'args': ['--use-gpu-in-tests']},
  'gles2_conform_d3d9_test': {
    'win_only': True,
    'args': [
      '--use-gpu-in-tests',
      '--use-angle=d3d9',
    ],
    'test': 'gles2_conform_test',
  },
  'gles2_conform_gl_test': {
    'win_only': True,
    'args': [
      '--use-gpu-in-tests',
      '--use-angle=gl',
      '--disable-gpu-sandbox',
    ],
    'test': 'gles2_conform_test',
  }
}

DEQP_GTESTS = {
  'angle_deqp_gles2_tests': {'swarming_shards': 4},
  'angle_deqp_gles3_tests': {'swarming_shards': 12},
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
    'win_only': True,
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=d3d9',
    ],
  },
  'webgl_conformance_gl_tests': {
    'win_only': True,
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=gl',
    ],
  },
  'webgl2_conformance_tests': {
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

def generate_gtest(tester_config, test, test_config):
  result = copy.deepcopy(test_config)
  if result.get('win_only'):
    if tester_config['os_type'] != 'win':
      return None
    # Don't print this in the JSON.
    result.pop('win_only')
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
  # print "generating " + test
  return result

def generate_telemetry_test(tester_config, test, test_config):
  if test_config.get('win_only'):
    if tester_config['os_type'] != 'win':
      return None
  test_args = ['-v']
  # --expose-gc allows the WebGL conformance tests to more reliably
  # reproduce GC-related bugs in the V8 bindings.
  extra_browser_args_string = (
      '--enable-logging=stderr --js-flags=--expose-gc')
  if 'extra_browser_args' in test_config:
    extra_browser_args_string += ' ' + ' '.join(
        test_config['extra_browser_args'])
  test_args.append('--extra-browser-args=\"' + extra_browser_args_string +
                   '\"')
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

def generate_gtests(tester_config, test_dictionary):
  # The relative ordering of some of the tests is important to
  # minimize differences compared to the handwritten JSON files, since
  # Python's sorts are stable and there are some tests with the same
  # key (see gles2_conform_d3d9_test and similar variants). Avoid
  # losing the order by avoiding coalescing the dictionaries into one.
  gtests = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_gtest(tester_config, test_name, test_config)
    if test:
      # generate_gtest may veto the test generation on this platform.
      gtests.append(test)
  return gtests

def generate_all_tests():
  tests = {}
  for builder in BUILDERS:
    tests[builder] = {}
  for name, config in TESTERS.iteritems():
    gtests = []
    if config.get('deqp'):
      gtests.extend(generate_gtests(config, DEQP_GTESTS))
    else:
      gtests.extend(generate_gtests(config, COMMON_GTESTS))
      if config['build_config'] == 'Release':
        gtests.extend(generate_gtests(config, RELEASE_ONLY_GTESTS))
      # For the moment we're only generating the FYI waterfall's tests.
      gtests.extend(generate_gtests(config, FYI_ONLY_GTESTS))
    isolated_scripts = []
    if not config.get('deqp'):
      for test_name, test_config in sorted(TELEMETRY_TESTS.iteritems()):
        test = generate_telemetry_test(config, test_name, test_config)
        if test:
          isolated_scripts.append(test)
    cur_tests = {}
    if gtests:
      cur_tests['gtest_tests'] = sorted(gtests, key=lambda x: x['test'])
    if isolated_scripts:
      cur_tests['isolated_scripts'] = sorted(
          isolated_scripts, key=lambda x: x['name'])
    tests[name] = cur_tests
  tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
  tests['AAAAA2 See generate_buildbot_json.py to make changes'] = {}
  print json.dumps(tests, indent=2, separators=(',', ': '), sort_keys=True)

if __name__ == "__main__":
  sys.exit(generate_all_tests())
