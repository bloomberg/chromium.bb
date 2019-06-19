module.exports = function(grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),

    clean: ['out/'],

    mkdir: {
      'out': {
        options: {
          create: ['out']
        }
      }
    },

    run: {
      'generate-listings': {
        cmd: 'tools/gen.js',
        args: [ 'cts', 'unittests', 'demos' ]
      },
      'test': {
        cmd: 'tools/run.js',
        args: [ 'unittests' ]
      },
      'build-out': {
        cmd: 'npx',
        args: [
          'babel',
          '--source-maps', 'true',
          '--extensions', '.ts',
          '--out-dir', 'out/',
          'src/',
        ]
      },
      'build-shaderc': {
        cmd: 'npx',
        args: [
          'babel',
          '--plugins', 'babel-plugin-transform-commonjs-es2015-modules',
          'node_modules/@webgpu/shaderc/dist/index.js',
          '-o', 'out/shaderc.js',
        ]
      },
      'lint': {
        cmd: 'npx',
        args: ['tslint', '--project', '.']
      },
      'fix': {
        cmd: 'npx',
        args: ['tslint', '--project', '.', '--fix']
      },
      'check-git-is-clean': {
        cmd: 'git',
        args: ['diff-index', '--quiet', 'HEAD']
      }
    },

    'http-server': {
      '.': {
        root: '.',
        port: 8080,
        host: '127.0.0.1',
        cache: 5,
      }
    },

    ts: {
      'check': {
        tsconfig: {
          tsconfig: 'tsconfig.json',
          passThrough: true,
        },
      },

      'out/': {
        tsconfig: {
          tsconfig: 'tsconfig.web.json',
          passThrough: true,
        },
      },
    },
  });

  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-http-server');
  grunt.loadNpmTasks('grunt-mkdir');
  grunt.loadNpmTasks('grunt-run');
  grunt.loadNpmTasks('grunt-ts');

  const publishedTasks = [];
  function publishTask(name, desc, deps) {
    grunt.registerTask(name, deps);
    publishedTasks.push({ name, desc });
  }

  publishTask('check', 'Check Typescript build', [
    'ts:check',
  ]);
  publishedTasks.push({ name: 'run:{lint,fix}', desc: 'Run tslint' });
  publishTask('build', 'Build out/', [
    'mkdir:out',
    'run:build-shaderc',
    'run:build-out',
    'run:generate-listings',
  ]);
  publishTask('serve', 'Serve out/ on 127.0.0.1:8080', [
    'http-server:.',
  ]);
  publishedTasks.push({ name: 'clean', desc: 'Clean out/' });

  grunt.registerTask('default', '', () => {
    console.log('Available tasks (see grunt --help for info):');
    for (const { name, desc } of publishedTasks) {
      console.log(`$ grunt ${name}`);
      console.log(`  ${desc}`);
    }
  });

  publishTask('presubmit', 'Run all presubmit checks', [
    'check',
    'build',
    'run:test',

    // 'format',  // TODO
    // 'run:fix',  // TODO
    'run:check-git-is-clean',
  ]);
};
