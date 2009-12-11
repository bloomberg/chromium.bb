{
  'targets': [
    { 'target_name': 'cros_api',
      'type': '<(library)',
      'sources': [
        '../../../third_party/cros/chromeos_power.h',
        '../../../third_party/cros/chromeos_network.h',
        '../../../third_party/cros/load.cc',
      ],
      'include_dirs': [
        '../../../third_party/cros',
        '../../../third_party',
        '../../..',
      ],
    },
  ],
 }
