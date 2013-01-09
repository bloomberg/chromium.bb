{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'SEARCH' : [
      '../../../../ppapi/cpp',
      '../../../../ppapi/utility',
      '../../../../ppapi/utility/graphics',
      '../../../../ppapi/utility/threading',
      '../../../../ppapi/utility/websocket',
  ],
  'TARGETS': [
    {
      'NAME' : 'ppapi_cpp',
      'TYPE' : 'lib',
      'SOURCES' : [
          'ppp_entrypoints.cc',
          'array_output.cc',
          'audio.cc',
          'audio_config.cc',
          'core.cc',
          'file_io.cc',
          'file_ref.cc',
          'file_system.cc',
          'fullscreen.cc',
          'graphics_2d.cc',
          'graphics_3d.cc',
          'graphics_3d_client.cc',
          'image_data.cc',
          'input_event.cc',
          'instance.cc',
          'instance_handle.cc',
          'lock.cc',
          'message_loop.cc',
          'module.cc',
          'mouse_cursor.cc',
          'mouse_lock.cc',
          'rect.cc',
          'resource.cc',
          'url_loader.cc',
          'url_request_info.cc',
          'url_response_info.cc',
          'var.cc',
          'var_array_buffer.cc',
          'view.cc',
          'websocket.cc',


          # Utility sources.
          'paint_aggregator.cc',
          'paint_manager.cc',
          'simple_thread.cc',
          'websocket_api.cc',
      ],
    }
  ],
  'DEST': 'src',
  'NAME': 'ppapi_cpp',
}

