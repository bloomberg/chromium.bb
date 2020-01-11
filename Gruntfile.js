module.exports = function (grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),

    clean: {
      constants: ['src/constants.ts'],
      out: ['out/', 'out-wpt'],
    },

    mkdir: {
      out: {
        options: {
          create: ['out'],
        },
      },
    },

    run: {
      'generate-version': {
        cmd: 'node',
        args: ['tools/gen_version'],
      },
      'generate-listings': {
        cmd: 'node',
        args: ['tools/gen_listings', 'cts', 'unittests'],
      },
      'generate-wpt-cts-html': {
        cmd: 'node',
        args: ['tools/gen_wpt_cts_html', 'out-wpt/cts.html', 'templates/cts.html'],
      },
      test: {
        cmd: 'node',
        args: ['tools/run', 'unittests:'],
      },
      'build-out': {
        cmd: 'node',
        args: [
          'node_modules/@babel/cli/bin/babel',
          '--source-maps',
          'true',
          '--extensions',
          '.ts',
          '--out-dir',
          'out/',
          'src/',
        ],
      },
      'gts-check': {
        cmd: 'node',
        args: ['node_modules/gts/build/src/cli', 'check'],
      },
      'gts-fix': {
        cmd: 'node',
        args: ['node_modules/gts/build/src/cli', 'fix'],
      },
    },

    copy: {
      'webgpu-constants': {
        files: [
          {
            expand: true,
            cwd: 'node_modules/@webgpu/types/src',
            src: 'constants.ts',
            dest: 'src/',
          },
        ],
      },
      glslang: {
        files: [
          {
            expand: true,
            cwd: 'node_modules/@webgpu/glslang/dist/web-devel',
            src: 'glslang.{js,wasm}',
            dest: 'out/',
          },
        ],
      },
      'out-wpt': {
        files: [
          { expand: true, cwd: '.', src: 'LICENSE.txt', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'constants.js', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'framework/**/*.js', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'suites/cts/**/*.js', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'runtime/wpt.js', dest: 'out-wpt/' },
          { expand: true, cwd: 'out', src: 'runtime/helper/**/*.js', dest: 'out-wpt/' },
        ],
      },
    },

    'http-server': {
      '.': {
        root: '.',
        port: 8080,
        host: '127.0.0.1',
        cache: -1,
      },
    },

    ts: {
      check: {
        tsconfig: {
          tsconfig: 'tsconfig.json',
          passThrough: true,
        },
      },
    },
  });

  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-contrib-copy');
  grunt.loadNpmTasks('grunt-http-server');
  grunt.loadNpmTasks('grunt-mkdir');
  grunt.loadNpmTasks('grunt-run');
  grunt.loadNpmTasks('grunt-ts');

  const helpMessageTasks = [];
  function registerTaskAndAddToHelp(name, desc, deps) {
    grunt.registerTask(name, deps);
    addExistingTaskToHelp(name, desc);
  }
  function addExistingTaskToHelp(name, desc) {
    helpMessageTasks.push({ name, desc });
  }

  grunt.registerTask('set-quiet-mode', () => {
    grunt.log.write('Running other tasks');
    require('quiet-grunt');
  });

  grunt.registerTask('prebuild', 'Pre-build tasks (clean and re-copy)', [
    'clean',
    'mkdir:out',
    'copy:webgpu-constants',
    'copy:glslang',
  ]);
  grunt.registerTask('compile', 'Compile and generate (no checks, no WPT)', [
    'run:build-out',
    'run:generate-version',
    'run:generate-listings',
  ]);
  grunt.registerTask('generate-wpt', 'Generate out-wpt/', [
    'copy:out-wpt',
    'run:generate-wpt-cts-html',
  ]);
  grunt.registerTask('compile-done-message', () => {
    process.stderr.write('\nBuild completed! Running checks/tests');
  });

  registerTaskAndAddToHelp('pre', 'Run all presubmit checks: build+typecheck+test+lint', [
    'set-quiet-mode',
    'wpt',
    'run:gts-check',
  ]);
  registerTaskAndAddToHelp('test', 'Quick development build: build+typecheck+test', [
    'set-quiet-mode',
    'prebuild',
    'compile',
    'compile-done-message',
    'ts:check',
    'run:test',
  ]);
  registerTaskAndAddToHelp('wpt', 'Build for WPT: build+typecheck+test+wpt', [
    'set-quiet-mode',
    'prebuild',
    'compile',
    'generate-wpt',
    'compile-done-message',
    'ts:check',
    'run:test',
  ]);
  registerTaskAndAddToHelp('check', 'Typecheck and lint', [
    'set-quiet-mode',
    'copy:webgpu-constants',
    'ts:check',
    'run:gts-check',
  ]);

  registerTaskAndAddToHelp('serve', 'Serve out/ on 127.0.0.1:8080', ['http-server:.']);
  registerTaskAndAddToHelp('fix', 'Fix lint and formatting', ['run:gts-fix']);

  grunt.registerTask('default', '', () => {
    console.error('\nAvailable tasks (see grunt --help for info):');
    for (const { name, desc } of helpMessageTasks) {
      console.error(`$ grunt ${name}`);
      console.error(`  ${desc}`);
    }
  });
};
