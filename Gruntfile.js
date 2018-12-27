module.exports = function(grunt) {
  // Project configuration.
  grunt.initConfig({
    pkg: grunt.file.readJSON("package.json"),

    ts: {
      "out-web": {
        tsconfig: {
          tsconfig: "tsconfig.json",
          passThrough: true,
        },
        options: {
          additionalFlags: "--noEmit false --outDir out-web/",
        }
      },
      "out-node": {
        tsconfig: {
          tsconfig: "tsconfig.json",
          passThrough: true,
        },
        options: {
          additionalFlags: "--noEmit false --outDir out-node/ --module commonjs",
        }
      },
    },

    tslint: {
      options: {
        configuration: "tslint.json",
      },
      files: {
        src: [ "src/**/*.ts" ],
      },
    },
  });

  grunt.loadNpmTasks("grunt-ts");
  grunt.loadNpmTasks("grunt-tslint");

  grunt.registerTask("web", ["ts:out-web"]);
  grunt.registerTask("node", ["ts:out-node"]);
  grunt.registerTask("default", ["web", "node"]);
};
