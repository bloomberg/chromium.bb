{
  'variables': {
    'chromium_code': 1,
    'chrome_frame_grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome_frame',

    # TODO(sgk): eliminate this; see comment in build/common.gypi
    'msvs_debug_link_incremental': '1',
  },
  'target_defaults': {
    'type': 'loadable_module',
    'dependencies': [
      '../chrome_frame.gyp:chrome_frame_strings',
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'BaseAddress': '0x3CF00000',
        'OutputFile': '$(OutDir)\\locales\\$(ProjectName).dll',
        'LinkIncremental': '1',  # 1 == No
        'LinkTimeCodeGeneration': '0',
        'ResourceOnlyDLL': 'true',
      },
    },
    'defines': [
      '_USRDLL',
      'GENERATED_RESOURCES_DLL_EXPORTS',
    ],
    'include_dirs': [
      '<(chrome_frame_grit_out_dir)',
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'am',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_am.rc',
          ],
        },
        {
          'target_name': 'ar',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ar.rc',
          ],
        },
        {
          'target_name': 'bg',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_bg.rc',
          ],
        },
        {
          'target_name': 'bn',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_bn.rc',
          ],
        },
        {
          'target_name': 'ca',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ca.rc',
          ],
        },
        {
          'target_name': 'cs',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_cs.rc',
          ],
        },
        {
          'target_name': 'da',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_da.rc',
          ],
        },
        {
          'target_name': 'de',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_de.rc',
          ],
        },
        {
          'target_name': 'el',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_el.rc',
          ],
        },
        {
          'target_name': 'en-GB',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_en-GB.rc',
          ],
        },
        {
          'target_name': 'en-US',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_en-US.rc',
          ],
        },
        {
          'target_name': 'es-419',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_es-419.rc',
          ],
        },
        {
          'target_name': 'es',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_es.rc',
          ],
        },
        {
          'target_name': 'et',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_et.rc',
          ],
        },
        {
          'target_name': 'fa',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_fa.rc',
          ],
        },
        {
          'target_name': 'fi',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_fi.rc',
          ],
        },
        {
          'target_name': 'fil',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_fil.rc',
          ],
        },
        {
          'target_name': 'fr',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_fr.rc',
          ],
        },
        {
          'target_name': 'gu',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_gu.rc',
          ],
        },
        {
          'target_name': 'he',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_he.rc',
          ],
        },
        {
          'target_name': 'hi',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_hi.rc',
          ],
        },
        {
          'target_name': 'hr',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_hr.rc',
          ],
        },
        {
          'target_name': 'hu',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_hu.rc',
          ],
        },
        {
          'target_name': 'id',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_id.rc',
          ],
        },
        {
          'target_name': 'it',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_it.rc',
          ],
        },
        {
          'target_name': 'ja',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ja.rc',
          ],
        },
        {
          'target_name': 'kn',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_kn.rc',
          ],
        },
        {
          'target_name': 'ko',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ko.rc',
          ],
        },
        {
          'target_name': 'lt',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_lt.rc',
          ],
        },
        {
          'target_name': 'lv',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_lv.rc',
          ],
        },
        {
          'target_name': 'ml',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ml.rc',
          ],
        },
        {
          'target_name': 'mr',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_mr.rc',
          ],
        },
        {
          'target_name': 'nb',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_nb.rc',
          ],
        },
        {
          'target_name': 'nl',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_nl.rc',
          ],
        },
        {
          'target_name': 'pl',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_pl.rc',
          ],
        },
        {
          'target_name': 'pt-BR',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_pt-BR.rc',
          ],
        },
        {
          'target_name': 'pt-PT',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_pt-PT.rc',
          ],
        },
        {
          'target_name': 'ro',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ro.rc',
          ],
        },
        {
          'target_name': 'ru',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ru.rc',
          ],
        },
        {
          'target_name': 'sk',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_sk.rc',
          ],
        },
        {
          'target_name': 'sl',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_sl.rc',
          ],
        },
        {
          'target_name': 'sr',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_sr.rc',
          ],
        },
        {
          'target_name': 'sv',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_sv.rc',
          ],
        },
        {
          'target_name': 'sw',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_sw.rc',
          ],
        },
        {
          'target_name': 'ta',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_ta.rc',
          ],
        },
        {
          'target_name': 'te',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_te.rc',
          ],
        },
        {
          'target_name': 'th',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_th.rc',
          ],
        },
        {
          'target_name': 'tr',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_tr.rc',
          ],
        },
        {
          'target_name': 'uk',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_uk.rc',
          ],
        },
        {
          'target_name': 'vi',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_vi.rc',
          ],
        },
        {
          'target_name': 'zh-CN',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_zh-CN.rc',
          ],
        },
        {
          'target_name': 'zh-TW',
          'sources': [
            '<(chrome_frame_grit_out_dir)/chrome_frame_dialogs_zh-TW.rc',
          ],
        },
      ],
    }],
  ],
}
