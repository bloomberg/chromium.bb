{
  'targets': [
    {
      # GN version: //content/browser/service_worker:proto
      'target_name': 'proto',
      'type': 'static_library',
      'sources': [
        'service_worker_cache.proto',
        'service_worker_database.proto',
      ],
      'variables': {
        'proto_in_dir': '.',
        'proto_out_dir': 'content/browser/service_worker',
      },
      'includes': [ '../../../build/protoc.gypi' ]
    },
  ],
}
