module.exports = function (api) {
  api.cache(true);
  return {
    presets: ['@babel/preset-typescript'],
    plugins: [
      '@babel/plugin-proposal-class-properties',
      '@babel/plugin-syntax-dynamic-import',
      '@babel/plugin-syntax-import-meta',
      'const-enum',
      'macros',
      [
        'add-header-comment',
        {
          header: ['AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts'],
        },
      ],
    ],
    compact: false,
    shouldPrintComment: val => !/tslint:/.test(val),
  };
};
