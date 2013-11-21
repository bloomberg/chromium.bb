{
  'targets': [
    {
      'target_name': 'cast_vp8_encoder',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
         '<(DEPTH)/third_party/',
      ],
      'sources': [
        'vp8_encoder.cc',
        'vp8_encoder.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
      ],
    },
  ],
}
