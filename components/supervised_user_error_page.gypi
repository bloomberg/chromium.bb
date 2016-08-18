{
  'targets': [
    {
      # GN version: //components/supervised_user_error_page
      'target_name': 'supervised_user_error_page',
      'type': 'static_library',
      'dependencies': [
        'components_resources.gyp:components_resources',
        'components_strings.gyp:components_strings',
        '../base/base.gyp:base',
        '../ui/base/ui_base.gyp:ui_base'
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'supervised_user_error_page/supervised_user_error_page.cc',
        'supervised_user_error_page/supervised_user_error_page.h',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          # GN version: //components/supervised_user_error_page:gin
          'target_name': 'supervised_user_error_page_gin',
          'type': 'static_library',
          'dependencies': [
            'components.gyp:web_restrictions_interfaces',
            '../content/content.gyp:content_renderer',
          ],
          'sources': [
            'supervised_user_error_page/supervised_user_gin_wrapper.cc',
            'supervised_user_error_page/supervised_user_gin_wrapper.h',
          ],
        }
      ],
    }],
  ],
}
