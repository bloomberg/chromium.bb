{
  'includes': [
    '../third_party/WebKit/Source/bindings/dart/gyp/overrides.gypi',
  ],
  'variables': {
    # Fixes mysterious forge issue.
    'use_custom_freetype': 0,
  },
}
