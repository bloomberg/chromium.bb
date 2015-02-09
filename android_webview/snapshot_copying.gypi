{
  'variables': {
    'snapshot_additional_input_paths': [],
    'snapshot_copy_files': [],
    'conditions': [
      ['v8_use_external_startup_data==1 and (target_arch=="arm" or target_arch=="x86" or target_arch=="mips")', {
        'snapshot_additional_input_paths': [
          '<(asset_location)/natives_blob_32.bin',
          '<(asset_location)/snapshot_blob_32.bin',
        ],
        'snapshot_copy_files': [
          '<(PRODUCT_DIR)/natives_blob_32.bin',
          '<(PRODUCT_DIR)/snapshot_blob_32.bin',
        ],
      }],
      ['v8_use_external_startup_data==1 and (target_arch=="arm64" or target_arch=="x86_64" or target_arch=="mips64")', {
        'snapshot_additional_input_paths': [
          '<(asset_location)/natives_blob_64.bin',
          '<(asset_location)/snapshot_blob_64.bin',
        ],
        'snapshot_copy_files': [
          '<(PRODUCT_DIR)/natives_blob_64.bin',
          '<(PRODUCT_DIR)/snapshot_blob_64.bin',
        ],
      }],
    ],
  },
}
